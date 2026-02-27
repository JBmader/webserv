/* ************************************************************************** */
/*  CLIENT.HPP                                                                */
/*  FR: Classe Client - machine a etats pour une connexion HTTP               */
/*  EN: Client class - state machine for an HTTP connection                   */
/* ************************************************************************** */

#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "Webserv.hpp"
# include "Request.hpp"
# include "Response.hpp"
# include "CGI.hpp"
# include "Config.hpp"

class Client {
public:
	/*
	** FR: Les 5 etats du cycle de vie d'une connexion client:
	**     READING     - reception de la requete depuis le socket
	**     PROCESSING  - routage et construction de la reponse
	**     CGI_RUNNING - attente de la fin d'un script CGI
	**     SENDING     - envoi de la reponse au client
	**     DONE        - connexion terminee, prete a etre supprimee
	** EN: 5 lifecycle states of a client connection:
	**     READING     - receiving request data from socket
	**     PROCESSING  - routing and building the response
	**     CGI_RUNNING - waiting for CGI script to finish
	**     SENDING     - sending response to client
	**     DONE        - connection finished, ready for cleanup
	*/
	enum State {
		STATE_READING,
		STATE_PROCESSING,
		STATE_CGI_RUNNING,
		STATE_SENDING,
		STATE_DONE
	};

	Client(int fd, const std::string& host, int port, size_t maxBodySize);
	~Client();

	// FR: Entrees/Sorties / EN: I/O
	bool	readData();
	bool	sendData();

	// FR: Traitement de la requete / EN: Request processing
	void	process(const Config& config);

	// FR: Accesseurs / EN: Accessors
	int			getFd() const;
	State		getState() const;
	time_t		getLastActivity() const;
	bool		hasTimedOut() const;
	CGI*		getCGI();

	// FR: Integration avec poll() / EN: For poll integration
	bool		wantsToRead() const;
	bool		wantsToWrite() const;

	// FR: Gestion du processus CGI / EN: CGI management
	void		finalizeCGI();

private:
	int					_fd;				// FR: Socket client / EN: Client socket fd
	std::string			_host;
	int					_port;
	State				_state;				// FR: Etat courant / EN: Current state
	Request				_request;			// FR: Parseur de requete / EN: Request parser
	Response			_response;			// FR: Constructeur de reponse / EN: Response builder
	CGI*				_cgi;				// FR: Gestionnaire CGI (NULL si pas de CGI) / EN: CGI handler (NULL if no CGI)
	const ServerConfig*	_matchedServer;		// FR: Config serveur correspondante / EN: Matched server config
	size_t				_sendOffset;		// FR: Position dans l'envoi partiel / EN: Offset for partial send
	time_t				_lastActivity;		// FR: Timestamp de derniere activite / EN: Last activity timestamp

	Client(const Client&);
	Client& operator=(const Client&);
};

#endif
