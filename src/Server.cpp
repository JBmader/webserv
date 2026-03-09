#include "Server.hpp"
#include "Utils.hpp"

static bool g_running = true;

static void signalHandler(int sig) {
	(void)sig;
	g_running = false;
}

Server::Server(const Config& config) : _config(config), _running(false) {}

Server::~Server() {
	// Clean up clients
	for (std::map<int, Client*>::iterator it = _clients.begin();
		 it != _clients.end(); ++it) {
		delete it->second;
	}
	_clients.clear();

	// Close listen sockets
	for (size_t i = 0; i < _listenFds.size(); ++i)
		close(_listenFds[i]);
	_listenFds.clear();
	_pollfds.clear();
}

void Server::stop() {
	_running = false;
}

void Server::run() {
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	_createListenSockets();

	if (_listenFds.empty())
		throw std::runtime_error("No listen sockets could be created");

	_running = true;
	LOG_INFO("Server started");

	_pollLoop();

	LOG_INFO("Server stopped");
}

void Server::_createListenSockets() {
	const std::vector<ServerConfig>& servers = _config.getServers();
	std::set<std::pair<std::string, int> > created;

	for (size_t i = 0; i < servers.size(); ++i) {
		std::pair<std::string, int> addr(servers[i].host, servers[i].port);
		if (created.find(addr) != created.end())
			continue; // Already listening on this address

		int fd = _createSocket(servers[i].host, servers[i].port);
		if (fd >= 0) {
			_listenFds.push_back(fd);
			_listenInfo[fd] = addr;
			_addPollFd(fd, POLLIN);
			created.insert(addr);
			LOG_INFO("Listening on " << servers[i].host << ":" << servers[i].port);
		}
	}
}

int Server::_createSocket(const std::string& host, int port) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		LOG_ERROR("socket() failed: " << strerror(errno));
		return -1;
	}

	// Allow port reuse
	int opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		LOG_ERROR("setsockopt() failed: " << strerror(errno));
		close(fd);
		return -1;
	}

	// Non-blocking
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		LOG_ERROR("fcntl() failed: " << strerror(errno));
		close(fd);
		return -1;
	}

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (host == "0.0.0.0" || host.empty()) {
		addr.sin_addr.s_addr = INADDR_ANY;
	} else {
		addr.sin_addr.s_addr = inet_addr(host.c_str());
		if (addr.sin_addr.s_addr == INADDR_NONE) {
			// Try resolving hostname
			struct addrinfo hints, *res;
			std::memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			if (getaddrinfo(host.c_str(), NULL, &hints, &res) == 0) {
				addr.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
				freeaddrinfo(res);
			} else {
				LOG_ERROR("Cannot resolve host: " << host);
				close(fd);
				return -1;
			}
		}
	}

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		LOG_ERROR("bind() failed on " << host << ":" << port << ": " << strerror(errno));
		close(fd);
		return -1;
	}

	if (listen(fd, SOMAXCONN) < 0) {
		LOG_ERROR("listen() failed: " << strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

// ─── Poll helpers ────────────────────────────────────────────────────────────

void Server::_addPollFd(int fd, short events) {
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0;
	_pollfds.push_back(pfd);
}

void Server::_removePollFd(int fd) {
	for (std::vector<struct pollfd>::iterator it = _pollfds.begin();
		 it != _pollfds.end(); ++it) {
		if (it->fd == fd) {
			_pollfds.erase(it);
			return;
		}
	}
}

// ─── Event loop ──────────────────────────────────────────────────────────────

void Server::_pollLoop() {
	while (_running && g_running) {
		// Update poll events for clients
		for (size_t i = 0; i < _pollfds.size(); ++i) {
			int fd = _pollfds[i].fd;
			std::map<int, Client*>::iterator it = _clients.find(fd);
			if (it != _clients.end()) {
				short events = 0;
				if (it->second->wantsToRead())
					events |= POLLIN;
				if (it->second->wantsToWrite())
					events |= POLLOUT;
				if (it->second->getState() == Client::STATE_CGI_RUNNING)
					events = 0; // Client socket not active during CGI
				_pollfds[i].events = events;
			}
		}

		int ready = poll(&_pollfds[0], _pollfds.size(), 1000);
		if (ready < 0) {
			if (!_running || !g_running)
				break;
			continue; // interrupted by signal
		}

		// Process events (iterate on a copy of size since vector may change)
		size_t pollSize = _pollfds.size();
		for (size_t i = 0; i < pollSize && i < _pollfds.size(); ++i) {
			if (_pollfds[i].revents == 0)
				continue;

			int fd = _pollfds[i].fd;
			short revents = _pollfds[i].revents;

			// Check if it's a listen socket
			bool isListen = false;
			for (size_t j = 0; j < _listenFds.size(); ++j) {
				if (_listenFds[j] == fd) {
					isListen = true;
					break;
				}
			}

			if (isListen) {
				if (revents & POLLIN)
					_acceptConnection(fd);
				continue;
			}

			// Check if it's a CGI pipe fd
			bool isCGIPipe = false;
			for (std::map<int, Client*>::iterator it = _clients.begin();
				 it != _clients.end(); ++it) {
				CGI* cgi = it->second->getCGI();
				if (cgi) {
					if (cgi->getOutputFd() == fd) {
						if (revents & (POLLIN | POLLHUP)) {
							_handleCGIRead(fd);
						}
						isCGIPipe = true;
						break;
					}
					if (cgi->getInputFd() == fd) {
						if (revents & POLLOUT) {
							_handleCGIWrite(fd);
						}
						isCGIPipe = true;
						break;
					}
				}
			}
			if (isCGIPipe)
				continue;

			// Client socket
			if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
				_removeClient(fd);
				// Adjust index since we removed from vector
				if (i > 0) --i;
				pollSize = _pollfds.size();
				continue;
			}

			if (revents & POLLIN)
				_handleClientRead(fd);

			// Re-check if client still exists (might have been removed)
			if (_clients.find(fd) == _clients.end())
				continue;

			if (revents & POLLOUT)
				_handleClientWrite(fd);
		}

		// Process clients that are in PROCESSING state
		for (std::map<int, Client*>::iterator it = _clients.begin();
			 it != _clients.end(); ++it) {
			if (it->second->getState() == Client::STATE_PROCESSING) {
				it->second->process(_config);
				// If CGI was started, add CGI pipes to poll
				CGI* cgi = it->second->getCGI();
				if (cgi) {
					if (cgi->getOutputFd() >= 0)
						_addPollFd(cgi->getOutputFd(), POLLIN);
					if (cgi->getInputFd() >= 0 && !cgi->isBodyWritten())
						_addPollFd(cgi->getInputFd(), POLLOUT);
				}
			}
		}

		// Check for CGI completions and timeouts
		for (std::map<int, Client*>::iterator it = _clients.begin();
			 it != _clients.end(); ++it) {
			CGI* cgi = it->second->getCGI();
			if (cgi && it->second->getState() == Client::STATE_CGI_RUNNING) {
				// Try to reap zombie children
				cgi->reapChild();
				if (cgi->isDone()) {
					it->second->finalizeCGI();
				} else if (cgi->checkTimeout()) {
					// Remove CGI pipes from poll
					if (cgi->getOutputFd() >= 0)
						_removePollFd(cgi->getOutputFd());
					if (cgi->getInputFd() >= 0)
						_removePollFd(cgi->getInputFd());
					cgi->kill();
					it->second->finalizeCGI();
				}
			}
		}

		// Check for completed/timed out clients
		_checkTimeouts();

		// Remove done clients
		std::vector<int> toRemove;
		for (std::map<int, Client*>::iterator it = _clients.begin();
			 it != _clients.end(); ++it) {
			if (it->second->getState() == Client::STATE_DONE)
				toRemove.push_back(it->first);
		}
		for (size_t i = 0; i < toRemove.size(); ++i)
			_removeClient(toRemove[i]);
	}
}

void Server::_acceptConnection(int listenFd) {
	struct sockaddr_in clientAddr;
	socklen_t addrLen = sizeof(clientAddr);
	int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &addrLen);

	if (clientFd < 0)
		return;

	// Set non-blocking
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0) {
		LOG_ERROR("fcntl() failed on client fd");
		close(clientFd);
		return;
	}

	// Get the host:port this listen socket is bound to
	std::string host = "0.0.0.0";
	int port = 8080;
	std::map<int, std::pair<std::string, int> >::iterator it = _listenInfo.find(listenFd);
	if (it != _listenInfo.end()) {
		host = it->second.first;
		port = it->second.second;
	}

	// Find max body size from server config matching this port
	size_t maxBody = 1048576; // 1MB default
	const ServerConfig* serverConf = _config.findServer("", port);
	if (serverConf)
		maxBody = serverConf->clientMaxBody;

	Client* client = new Client(clientFd, host, port, maxBody);
	_clients[clientFd] = client;
	_addPollFd(clientFd, POLLIN | POLLOUT);
}

void Server::_handleClientRead(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	if (!it->second->readData()) {
		_removeClient(fd);
	}
}

void Server::_handleClientWrite(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	if (!it->second->sendData()) {
		_removeClient(fd);
	}
}

void Server::_handleCGIRead(int fd) {
	for (std::map<int, Client*>::iterator it = _clients.begin();
		 it != _clients.end(); ++it) {
		CGI* cgi = it->second->getCGI();
		if (cgi && cgi->getOutputFd() == fd) {
			bool done = cgi->readOutput();
			if (done) {
				_removePollFd(fd);
			}
			return;
		}
	}
}

void Server::_handleCGIWrite(int fd) {
	for (std::map<int, Client*>::iterator it = _clients.begin();
		 it != _clients.end(); ++it) {
		CGI* cgi = it->second->getCGI();
		if (cgi && cgi->getInputFd() == fd) {
			bool done = cgi->writeBody();
			if (done) {
				_removePollFd(fd);
			}
			return;
		}
	}
}

void Server::_removeClient(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end()) {
		// Remove CGI pipes from poll if active
		CGI* cgi = it->second->getCGI();
		if (cgi) {
			if (cgi->getOutputFd() >= 0)
				_removePollFd(cgi->getOutputFd());
			if (cgi->getInputFd() >= 0)
				_removePollFd(cgi->getInputFd());
		}
		_removePollFd(fd);
		delete it->second;
		_clients.erase(it);
	}
}

void Server::_checkTimeouts() {
	std::vector<int> timedOut;
	for (std::map<int, Client*>::iterator it = _clients.begin();
		 it != _clients.end(); ++it) {
		if (it->second->hasTimedOut() &&
			it->second->getState() != Client::STATE_CGI_RUNNING) {
			timedOut.push_back(it->first);
		}
	}
	for (size_t i = 0; i < timedOut.size(); ++i) {
		LOG_WARN("Client timeout, disconnecting fd " << timedOut[i]);
		_removeClient(timedOut[i]);
	}
}
