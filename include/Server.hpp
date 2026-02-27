/* ************************************************************************** */
/*  SERVER.HPP                                                                */
/*  FR: Classe Server - boucle evenementielle poll() et gestion des sockets   */
/*  EN: Server class - poll() event loop and socket management                */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Client.hpp"

/*
** FR: Le coeur du serveur. Gere la boucle poll(), les sockets d'ecoute,
**     les connexions clients et les pipes CGI. Un seul appel a poll()
**     surveille TOUS les fds (ecoute + clients + CGI).
** EN: Server core. Manages poll() loop, listen sockets, client connections
**     and CGI pipes. Single poll() call monitors ALL fds (listen + client + CGI).
*/
class Server {
public:
	Server(const Config& config);
	~Server();

	void	run();
	void	stop();

private:
	const Config&					_config;
	bool							_running;
	// FR: Tableau de structures pollfd pour l'appel poll()
	// EN: Array of pollfd structures for the poll() call
	std::vector<struct pollfd>		_pollfds;
	// FR: File descriptors des sockets d'ecoute (un par host:port)
	// EN: Listen socket file descriptors (one per host:port)
	std::vector<int>				_listenFds;
	// FR: Association fd -> pointeur Client* pour chaque connexion active
	// EN: Map fd -> Client* pointer for each active connection
	std::map<int, Client*>			_clients;
	// FR: Association fd d'ecoute -> paire host:port correspondante
	// EN: Map listen fd -> corresponding host:port pair
	std::map<int, std::pair<std::string, int> >	_listenInfo;

	// FR: Initialisation / EN: Setup
	void	_createListenSockets();
	int		_createSocket(const std::string& host, int port);

	// FR: Boucle evenementielle / EN: Event loop internals
	void	_pollLoop();
	void	_acceptConnection(int listenFd);
	void	_handleClientRead(int fd);
	void	_handleClientWrite(int fd);
	void	_handleCGIRead(int fd);
	void	_handleCGIWrite(int fd);
	void	_removeClient(int fd);
	void	_checkTimeouts();

	// FR: Utilitaires poll / EN: Poll helpers
	void	_addPollFd(int fd, short events);
	void	_removePollFd(int fd);

	// FR: Declares prives pour empecher la copie (forme canonique)
	// EN: Private to prevent copying (canonical form)
	Server(const Server&);
	Server& operator=(const Server&);
};

#endif
