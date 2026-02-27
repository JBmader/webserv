/* ************************************************************************** */
/*  CLIENT.CPP                                                                */
/*  FR: Machine a etats gerant le cycle de vie d'une connexion client         */
/*  EN: State machine managing a client connection lifecycle                   */
/* ************************************************************************** */

#include "Client.hpp"
#include "Router.hpp"
#include "Utils.hpp"

/*
** FR: Initialise fd, host, port, etat READING, configure max body size
** EN: Initializes fd, host, port, state READING, sets max body size
*/
Client::Client(int fd, const std::string& host, int port, size_t maxBodySize)
	: _fd(fd), _host(host), _port(port), _state(STATE_READING),
	  _cgi(NULL), _matchedServer(NULL), _sendOffset(0), _lastActivity(time(NULL)) {
	_request.setMaxBodySize(maxBodySize);
}

/*
** FR: Libere CGI si actif, ferme le socket client
** EN: Frees CGI if active, closes client socket
*/
Client::~Client() {
	if (_cgi) {
		delete _cgi;
		_cgi = NULL;
	}
	if (_fd >= 0) {
		close(_fd);
		_fd = -1;
	}
}

/* FR: Retourne le descripteur de fichier du socket */
/* EN: Returns the socket file descriptor */
int Client::getFd() const { return _fd; }
/* FR: Retourne l'etat courant de la machine a etats */
/* EN: Returns the current state machine state */
Client::State Client::getState() const { return _state; }
/* FR: Retourne le timestamp de la derniere activite */
/* EN: Returns the timestamp of last activity */
time_t Client::getLastActivity() const { return _lastActivity; }
/* FR: Retourne le pointeur vers le processus CGI actif */
/* EN: Returns pointer to the active CGI process */
CGI* Client::getCGI() { return _cgi; }

/*
** FR: Verifie si le client est inactif depuis plus de CLIENT_TIMEOUT (60s)
** EN: Checks if client has been idle for more than CLIENT_TIMEOUT (60s)
*/
bool Client::hasTimedOut() const {
	return (time(NULL) - _lastActivity) >= CLIENT_TIMEOUT;
}

/*
** FR: Indique a poll() si on attend des donnees
** EN: Tells poll() whether we're waiting for data
*/
bool Client::wantsToRead() const {
	return _state == STATE_READING;
}

/*
** FR: Indique a poll() si on a des donnees a envoyer
** EN: Tells poll() whether we have data to send
*/
bool Client::wantsToWrite() const {
	return _state == STATE_SENDING;
}

/*
** FR: Lit donnees du socket via recv() non-bloquant, alimente le parser de
**     requete incrementalement. Retourne false si le client se deconnecte
**     (recv retourne 0 ou -1).
** EN: Reads from socket via non-blocking recv(), feeds request parser
**     incrementally. Returns false if client disconnects (recv returns 0 or -1).
*/
bool Client::readData() {
	char buf[BUFFER_SIZE];
	ssize_t n = recv(_fd, buf, sizeof(buf), 0);

	if (n <= 0)
		return false; // client disconnected or error

	_lastActivity = time(NULL);
	std::string data(buf, n);

	bool complete = _request.feed(data);
	if (complete) {
		if (_request.getState() == Request::PARSE_ERROR) {
			// Build error response and move to sending state
			ServerConfig defaultServer;
			_response.buildError(_request.getErrorCode(), defaultServer);
			_state = STATE_SENDING;
		} else {
			_state = STATE_PROCESSING;
		}
	}
	return true;
}

/*
** FR: Envoie la reponse HTTP via send() non-bloquant, gere l'offset pour
**     envoi partiel. Retourne false si send echoue.
** EN: Sends HTTP response via non-blocking send(), manages offset for
**     partial sends. Returns false if send fails.
*/
bool Client::sendData() {
	if (!_response.isReady())
		return true;

	const std::string& data = _response.getData();
	size_t remaining = data.size() - _sendOffset;

	if (remaining == 0) {
		_state = STATE_DONE;
		return true;
	}

	ssize_t n = send(_fd, data.c_str() + _sendOffset, remaining, 0);
	if (n <= 0)
		return false;

	_sendOffset += n;
	_lastActivity = time(NULL);

	if (_sendOffset >= data.size())
		_state = STATE_DONE;

	return true;
}

/*
** FR: ROUTAGE - extrait Host header, trouve le serveur et la location
**     correspondants, verifie si c'est un CGI ou une requete normale.
**     Lance le CGI si necessaire.
** EN: ROUTING - extracts Host header, finds matching server and location,
**     checks if it's CGI or normal request. Starts CGI if needed.
*/
void Client::process(const Config& config) {
	if (_state != STATE_PROCESSING)
		return;

	// Extract Host header to find the right server
	std::string hostHeader = _request.getHeader("Host");
	std::string hostname = hostHeader;
	// Remove port from Host header if present
	size_t colonPos = hostHeader.find(':');
	if (colonPos != std::string::npos)
		hostname = hostHeader.substr(0, colonPos);

	const ServerConfig* server = config.findServer(hostname, _port);
	if (!server) {
		// Use first server as default
		if (!config.getServers().empty())
			server = &config.getServers()[0];
		else {
			ServerConfig defaultServer;
			_response.buildError(500, defaultServer);
			_state = STATE_SENDING;
			return;
		}
	}

	// Set max body size for request retroactively (for error checking)
	_request.setMaxBodySize(server->clientMaxBody);

	const LocationConfig* location = server->findLocation(_request.getPath());
	if (!location) {
		_response.buildError(404, *server);
		_state = STATE_SENDING;
		return;
	}

	// Check if this is a CGI request
	std::string resolvedPath = Router::resolvePath(_request.getPath(), *location);
	if (Router::isCGI(*location, resolvedPath)) {
		// Start CGI
		_cgi = new CGI();
		if (!_cgi->execute(_request, *location, *server, resolvedPath)) {
			delete _cgi;
			_cgi = NULL;
			_response.buildError(500, *server);
			_state = STATE_SENDING;
			return;
		}
		// Store body for poll-driven writing to CGI stdin
		_cgi->setBody(_request.getBody());
		_matchedServer = server;
		_state = STATE_CGI_RUNNING;
		return;
	}

	// Normal request processing
	_response.build(_request, *server, *location);
	_state = STATE_SENDING;
}

/*
** FR: Recupere la sortie du CGI termine, construit la reponse HTTP a partir
**     de cette sortie. Si sortie vide -> erreur 502 Bad Gateway.
** EN: Retrieves output from completed CGI, builds HTTP response from it.
**     If output empty -> 502 Bad Gateway error.
*/
void Client::finalizeCGI() {
	if (!_cgi || !_cgi->isDone())
		return;

	ServerConfig defaultServer;
	const ServerConfig* server = _matchedServer ? _matchedServer : &defaultServer;

	if (_cgi->getOutput().empty()) {
		_response.buildError(502, *server);
	} else {
		_response.setCGIResponse(_cgi->getOutput(), *server);
	}

	delete _cgi;
	_cgi = NULL;
	_matchedServer = NULL;
	_state = STATE_SENDING;
}
