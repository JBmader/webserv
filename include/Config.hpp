/* ************************************************************************** */
/*  CONFIG.HPP                                                                */
/*  FR: Configuration du serveur - parsing style NGINX                        */
/*  EN: Server configuration - NGINX-style parsing                            */
/* ************************************************************************** */

#ifndef CONFIG_HPP
# define CONFIG_HPP

# include "Webserv.hpp"

// ─── LocationConfig ──────────────────────────────────────────────────────────
/*
** FR: Represente un bloc location { } du fichier de configuration.
**     Contient le chemin, la racine, l'index, les methodes autorisees,
**     l'autoindex, les redirections, le CGI et le chemin d'upload.
** EN: Represents a location { } block from the config file.
**     Contains path, root, index, allowed methods, autoindex,
**     redirects, CGI settings, and upload path.
*/

struct LocationConfig {
	std::string					path;			// e.g. "/", "/upload", "/cgi-bin"
	std::string					root;			// document root
	std::string					index;			// default index file
	std::set<std::string>		methods;		// allowed methods
	bool						autoindex;		// directory listing
	std::string					redirect;		// HTTP redirect (e.g. "301 /new")
	int							redirectCode;
	std::string					uploadPath;		// where to store uploads
	std::string					cgiExtension;	// e.g. ".py"
	std::string					cgiPath;		// e.g. "/usr/bin/python3"

	LocationConfig();
};

// ─── ServerConfig ────────────────────────────────────────────────────────────
/*
** FR: Represente un bloc server { } du fichier de configuration.
**     Contient host, port, server_name, pages d'erreur personnalisees,
**     limite de taille du body, et la liste des locations.
** EN: Represents a server { } block from the config file.
**     Contains host, port, server_name, custom error pages,
**     body size limit, and the list of locations.
*/

struct ServerConfig {
	std::string					host;			// e.g. "0.0.0.0"
	int							port;			// e.g. 8080
	std::vector<std::string>	serverNames;	// server_name directives
	std::map<int, std::string>	errorPages;		// code -> path
	size_t						clientMaxBody;	// max body size in bytes
	std::vector<LocationConfig>	locations;		// location blocks

	ServerConfig();
	const LocationConfig*	findLocation(const std::string& uri) const;
};

// ─── Config ──────────────────────────────────────────────────────────────────
/*
** FR: Parseur de fichier de configuration style NGINX.
**     Tokenize le fichier, parse les blocs server/location, valide la config.
** EN: NGINX-style config file parser.
**     Tokenizes the file, parses server/location blocks, validates config.
*/

class Config {
public:
	Config();
	~Config();

	void								parse(const std::string& filepath);
	const std::vector<ServerConfig>&	getServers() const;
	const ServerConfig*					findServer(const std::string& host, int port) const;

private:
	// FR: Liste des configurations serveur parsees
	// EN: List of parsed server configurations
	std::vector<ServerConfig>	_servers;
	// FR: Contenu brut du fichier de configuration
	// EN: Raw content of the configuration file
	std::string					_content;
	// FR: Position du curseur de parsing dans _content
	// EN: Parsing cursor position within _content
	size_t						_pos;

	// FR: Utilitaires de parsing / EN: Parsing helpers
	void			_removeComments();
	void			_skipWhitespace();
	std::string		_nextToken();
	std::string		_expectToken(const std::string& expected);
	void			_parseServer();
	void			_parseLocation(ServerConfig& server);
	void			_parseListen(ServerConfig& server, const std::string& value);
	void			_validateConfig();

	Config(const Config&);
	Config& operator=(const Config&);
};

#endif
