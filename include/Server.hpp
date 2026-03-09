#ifndef SERVER_HPP
# define SERVER_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Client.hpp"

class Server {
public:
	Server(const Config& config);
	~Server();

	void	run();
	void	stop();

private:
	const Config&					_config;
	bool							_running;
	std::vector<struct pollfd>		_pollfds;
	std::vector<int>				_listenFds;
	std::map<int, Client*>			_clients;
	// Map listen FDs to their host:port
	std::map<int, std::pair<std::string, int> >	_listenInfo;

	// Setup
	void	_createListenSockets();
	int		_createSocket(const std::string& host, int port);

	// Event loop internals
	void	_pollLoop();
	void	_acceptConnection(int listenFd);
	void	_handleClientRead(int fd);
	void	_handleClientWrite(int fd);
	void	_handleCGIRead(int fd);
	void	_handleCGIWrite(int fd);
	void	_removeClient(int fd);
	void	_checkTimeouts();

	// Poll helpers
	void	_addPollFd(int fd, short events);
	void	_removePollFd(int fd);

	Server(const Server&);
	Server& operator=(const Server&);
};

#endif
