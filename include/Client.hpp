#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "Webserv.hpp"
# include "Request.hpp"
# include "Response.hpp"
# include "CGI.hpp"
# include "Config.hpp"

class Client {
public:
	enum State {
		STATE_READING,
		STATE_PROCESSING,
		STATE_CGI_RUNNING,
		STATE_SENDING,
		STATE_DONE
	};

	Client(int fd, const std::string& host, int port, size_t maxBodySize);
	~Client();

	// I/O
	bool	readData();
	bool	sendData();

	// Processing
	void	process(const Config& config);

	// Accessors
	int			getFd() const;
	State		getState() const;
	time_t		getLastActivity() const;
	bool		hasTimedOut() const;
	CGI*		getCGI();

	// For poll integration
	bool		wantsToRead() const;
	bool		wantsToWrite() const;

	// CGI management
	void		finalizeCGI();

private:
	int					_fd;
	std::string			_host;
	int					_port;
	State				_state;
	Request				_request;
	Response			_response;
	CGI*				_cgi;
	const ServerConfig*	_matchedServer;
	size_t				_sendOffset;
	time_t				_lastActivity;

	Client(const Client&);
	Client& operator=(const Client&);
};

#endif
