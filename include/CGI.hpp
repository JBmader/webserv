#ifndef CGI_HPP
# define CGI_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp"

class CGI {
public:
	CGI();
	~CGI();

	// Setup and execute CGI
	bool	execute(const Request& req, const LocationConfig& location,
					const ServerConfig& server, const std::string& scriptPath);

	// Get pipe FDs for poll() integration
	int		getOutputFd() const;
	int		getInputFd() const;
	pid_t	getPid() const;
	bool	isDone() const;
	time_t	getStartTime() const;

	// Feed body to CGI stdin (non-blocking, called via poll POLLOUT)
	void	setBody(const std::string& body);
	bool	writeBody();
	bool	isBodyWritten() const;

	// Read CGI output (non-blocking)
	bool	readOutput();
	const std::string&	getOutput() const;

	// Cleanup
	void	closeFds();
	bool	checkTimeout();
	void	kill();
	bool	reapChild();

private:
	pid_t		_pid;
	int			_inputFd;		// pipe to CGI stdin
	int			_outputFd;		// pipe from CGI stdout
	std::string	_output;
	bool		_done;
	bool		_bodyWritten;
	time_t		_startTime;
	std::string	_bodyToWrite;
	size_t		_bodyWriteOffset;

	std::vector<std::string>	_buildEnv(const Request& req,
										  const LocationConfig& location,
										  const ServerConfig& server,
										  const std::string& scriptPath);

	CGI(const CGI&);
	CGI& operator=(const CGI&);
};

#endif
