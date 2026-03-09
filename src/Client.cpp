#include "Client.hpp"
#include "Router.hpp"
#include "Utils.hpp"

Client::Client(int fd, const std::string& host, int port, size_t maxBodySize)
	: _fd(fd), _host(host), _port(port), _state(STATE_READING),
	  _cgi(NULL), _matchedServer(NULL), _sendOffset(0), _lastActivity(time(NULL)) {
	_request.setMaxBodySize(maxBodySize);
}

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

int Client::getFd() const { return _fd; }
Client::State Client::getState() const { return _state; }
time_t Client::getLastActivity() const { return _lastActivity; }
CGI* Client::getCGI() { return _cgi; }

bool Client::hasTimedOut() const {
	return (time(NULL) - _lastActivity) >= CLIENT_TIMEOUT;
}

bool Client::wantsToRead() const {
	return _state == STATE_READING;
}

bool Client::wantsToWrite() const {
	return _state == STATE_SENDING;
}

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
