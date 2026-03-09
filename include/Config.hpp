#ifndef CONFIG_HPP
# define CONFIG_HPP

# include "Webserv.hpp"

// ─── LocationConfig ──────────────────────────────────────────────────────────

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

class Config {
public:
	Config();
	~Config();

	void								parse(const std::string& filepath);
	const std::vector<ServerConfig>&	getServers() const;
	const ServerConfig*					findServer(const std::string& host, int port) const;

private:
	std::vector<ServerConfig>	_servers;
	std::string					_content;
	size_t						_pos;

	// parsing helpers
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
