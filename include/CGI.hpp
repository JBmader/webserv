/* ************************************************************************** */
/*  CGI.HPP                                                                   */
/*  FR: Gestionnaire CGI - fork/pipe/execve pour scripts externes             */
/*  EN: CGI handler - fork/pipe/execve for external scripts                   */
/* ************************************************************************** */

#ifndef CGI_HPP
# define CGI_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp"

/*
** FR: Execute des scripts CGI via fork+pipe+execve.
**     Communication non-bloquante via poll().
**     Gestion du timeout (CGI_TIMEOUT) et des processus zombies.
** EN: Executes CGI scripts via fork+pipe+execve.
**     Non-blocking communication via poll().
**     Timeout handling (CGI_TIMEOUT) and zombie process reaping.
*/
class CGI {
public:
	CGI();
	~CGI();

	// FR: Configure et lance le script CGI / EN: Setup and execute CGI
	bool	execute(const Request& req, const LocationConfig& location,
					const ServerConfig& server, const std::string& scriptPath);

	// FR: Fds des pipes pour integration dans poll()
	// EN: Pipe fds for poll() integration
	int		getOutputFd() const;
	int		getInputFd() const;
	pid_t	getPid() const;
	bool	isDone() const;
	time_t	getStartTime() const;

	// FR: Envoie le body de la requete vers stdin du CGI (non-bloquant, via POLLOUT)
	// EN: Feed request body to CGI stdin (non-blocking, called via poll POLLOUT)
	void	setBody(const std::string& body);
	bool	writeBody();
	bool	isBodyWritten() const;

	// FR: Lit la sortie du CGI (non-bloquant) / EN: Read CGI output (non-blocking)
	bool	readOutput();
	const std::string&	getOutput() const;

	// FR: Nettoyage / EN: Cleanup
	void	closeFds();
	bool	checkTimeout();
	void	kill();
	bool	reapChild();

private:
	// FR: PID du processus enfant CGI / EN: CGI child process PID
	pid_t		_pid;
	// FR: Pipe vers stdin du CGI / EN: Pipe to CGI stdin
	int			_inputFd;
	// FR: Pipe depuis stdout du CGI / EN: Pipe from CGI stdout
	int			_outputFd;
	std::string	_output;
	bool		_done;
	bool		_bodyWritten;
	time_t		_startTime;
	// FR: Buffer du body a ecrire vers le CGI / EN: Body buffer to write to CGI
	std::string	_bodyToWrite;
	// FR: Position dans l'ecriture partielle du body
	// EN: Offset for partial body writing
	size_t		_bodyWriteOffset;

	std::vector<std::string>	_buildEnv(const Request& req,
										  const LocationConfig& location,
										  const ServerConfig& server,
										  const std::string& scriptPath);

	CGI(const CGI&);
	CGI& operator=(const CGI&);
};

#endif
