/* ************************************************************************** */
/*  CONFIG.CPP                                                                */
/*  FR: Parsing et validation du fichier de configuration style nginx.        */
/*      Gere les blocs server { } et location { } avec leurs directives.      */
/*  EN: Parsing and validation of nginx-style configuration file.             */
/*      Handles server { } and location { } blocks with their directives.     */
/* ************************************************************************** */

#include "Config.hpp"
#include "Utils.hpp"

// ─── LocationConfig ──────────────────────────────────────────────────────────

/*
** FR: Constructeur - valeurs par defaut: path "/", autoindex off,
**     autorise GET, POST et DELETE.
** EN: Constructor - default values: path "/", autoindex off,
**     allows GET, POST and DELETE.
*/
LocationConfig::LocationConfig()
	: path("/"), root(""), index("index.html"), autoindex(false),
	  redirectCode(0), cgiExtension(""), cgiPath("") {
	methods.insert("GET");
	methods.insert("POST");
	methods.insert("DELETE");
}

// ─── ServerConfig ────────────────────────────────────────────────────────────

/*
** FR: Constructeur - valeurs par defaut: host 0.0.0.0, port 8080,
**     max body size 1 Mo (1048576 octets).
** EN: Constructor - default values: host 0.0.0.0, port 8080,
**     max body size 1 MB (1048576 bytes).
*/
ServerConfig::ServerConfig()
	: host("0.0.0.0"), port(8080), clientMaxBody(1048576) {
}

/*
** FR: Recherche de location par plus long prefixe (longest-prefix match).
**     Retourne la location dont le path est le plus long prefixe de l'URI.
** EN: Location lookup by longest-prefix match.
**     Returns the location whose path is the longest prefix of the URI.
*/
const LocationConfig* ServerConfig::findLocation(const std::string& uri) const {
	const LocationConfig* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < locations.size(); ++i) {
		const std::string& locPath = locations[i].path;
		if (Utils::startsWith(uri, locPath)) {
			if (locPath.size() > bestLen) {
				bestLen = locPath.size();
				best = &locations[i];
			}
		}
	}
	return best;
}

// ─── Config ──────────────────────────────────────────────────────────────────

Config::Config() : _pos(0) {}

Config::~Config() {}

const std::vector<ServerConfig>& Config::getServers() const {
	return _servers;
}

/*
** FR: Cherche le serveur correspondant par server_name sur le port donne.
**     Fallback sur le premier serveur du port si aucun name ne match.
** EN: Finds matching server by server_name on the given port.
**     Falls back to the first server on the port if no name matches.
*/
const ServerConfig* Config::findServer(const std::string& host, int port) const {
	const ServerConfig* defaultServer = NULL;

	for (size_t i = 0; i < _servers.size(); ++i) {
		if (_servers[i].port == port) {
			if (defaultServer == NULL)
				defaultServer = &_servers[i];
			for (size_t j = 0; j < _servers[i].serverNames.size(); ++j) {
				if (_servers[i].serverNames[j] == host)
					return &_servers[i];
			}
		}
	}
	return defaultServer;
}

/*
** FR: Parse le fichier de config - lit le contenu, supprime les commentaires,
**     tokenize et parse les blocs server { }. Valide la config a la fin.
** EN: Parses config file - reads content, strips comments, tokenizes and
**     parses server { } blocks. Validates config at the end.
*/
void Config::parse(const std::string& filepath) {
	std::ifstream ifs(filepath.c_str());
	if (!ifs.is_open())
		throw std::runtime_error("Cannot open config file: " + filepath);

	std::ostringstream oss;
	oss << ifs.rdbuf();
	_content = oss.str();
	_pos = 0;
	ifs.close();

	_removeComments();

	while (_pos < _content.size()) {
		_skipWhitespace();
		if (_pos >= _content.size())
			break;
		std::string token = _nextToken();
		if (token == "server") {
			_parseServer();
		} else if (!token.empty()) {
			throw std::runtime_error("Unexpected token in config: '" + token + "'");
		}
	}

	if (_servers.empty())
		throw std::runtime_error("No server block found in config file");

	_validateConfig();
}

/*
** FR: Supprime les commentaires (tout ce qui suit '#' jusqu'a fin de ligne).
** EN: Strips comments (everything after '#' until end of line).
*/
void Config::_removeComments() {
	std::string result;
	result.reserve(_content.size());
	for (size_t i = 0; i < _content.size(); ++i) {
		if (_content[i] == '#') {
			while (i < _content.size() && _content[i] != '\n')
				++i;
		} else {
			result += _content[i];
		}
	}
	_content = result;
}

/*
** FR: Avance le curseur en sautant les espaces, tabs et retours a la ligne.
** EN: Advances cursor past spaces, tabs and newlines.
*/
void Config::_skipWhitespace() {
	while (_pos < _content.size() &&
		   (_content[_pos] == ' ' || _content[_pos] == '\t' ||
			_content[_pos] == '\r' || _content[_pos] == '\n'))
		++_pos;
}

/*
** FR: Extrait le prochain token - caracteres speciaux ({, }, ;) ou mot.
** EN: Extracts next token - special characters ({, }, ;) or word.
*/
std::string Config::_nextToken() {
	_skipWhitespace();
	if (_pos >= _content.size())
		return "";

	// Special single-char tokens
	if (_content[_pos] == '{' || _content[_pos] == '}' || _content[_pos] == ';') {
		std::string tok(1, _content[_pos]);
		++_pos;
		return tok;
	}

	// Regular token
	size_t start = _pos;
	while (_pos < _content.size() &&
		   _content[_pos] != ' ' && _content[_pos] != '\t' &&
		   _content[_pos] != '\r' && _content[_pos] != '\n' &&
		   _content[_pos] != '{' && _content[_pos] != '}' &&
		   _content[_pos] != ';')
		++_pos;
	return _content.substr(start, _pos - start);
}

/*
** FR: Lit le prochain token et verifie qu'il correspond a l'attendu.
**     Lance une exception si le token ne correspond pas.
** EN: Reads next token and verifies it matches the expected value.
**     Throws exception if token does not match.
*/
std::string Config::_expectToken(const std::string& expected) {
	std::string token = _nextToken();
	if (token != expected)
		throw std::runtime_error("Expected '" + expected + "', got '" + token + "'");
	return token;
}

/*
** FR: Parse la directive listen - accepte host:port ou port seul.
**     Valide que le port est dans le range 1-65535.
** EN: Parses listen directive - accepts host:port or port only.
**     Validates port is in range 1-65535.
*/
void Config::_parseListen(ServerConfig& server, const std::string& value) {
	size_t colon = value.rfind(':');
	if (colon != std::string::npos) {
		server.host = value.substr(0, colon);
		server.port = Utils::toInt(value.substr(colon + 1));
	} else {
		// Check if value is a number (port only) or host only
		bool isPort = true;
		for (size_t i = 0; i < value.size(); ++i) {
			if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
				isPort = false;
				break;
			}
		}
		if (isPort) {
			server.port = Utils::toInt(value);
		} else {
			server.host = value;
		}
	}
	if (server.port <= 0 || server.port > 65535)
		throw std::runtime_error("Invalid port: " + Utils::toString(server.port));
}

/*
** FR: Parse un bloc server { } avec toutes ses directives: listen,
**     server_name, error_page, client_max_body_size, location.
**     Ajoute une location "/" par defaut si aucune n'est definie.
** EN: Parses a server { } block with all directives: listen,
**     server_name, error_page, client_max_body_size, location.
**     Adds a default "/" location if none is defined.
*/
void Config::_parseServer() {
	_expectToken("{");

	ServerConfig server;
	std::string token;

	while (true) {
		token = _nextToken();
		if (token == "}")
			break;
		if (token.empty())
			throw std::runtime_error("Unexpected end of config in server block");

		if (token == "listen") {
			std::string value = _nextToken();
			_parseListen(server, value);
			_expectToken(";");
		}
		else if (token == "server_name") {
			while (true) {
				std::string name = _nextToken();
				if (name == ";")
					break;
				server.serverNames.push_back(name);
			}
		}
		else if (token == "error_page") {
			std::string codeStr = _nextToken();
			std::string path = _nextToken();
			int code = Utils::toInt(codeStr);
			server.errorPages[code] = path;
			_expectToken(";");
		}
		else if (token == "client_max_body_size") {
			std::string value = _nextToken();
			size_t multiplier = 1;
			char lastChar = value[value.size() - 1];
			if (lastChar == 'k' || lastChar == 'K') {
				multiplier = 1024;
				value = value.substr(0, value.size() - 1);
			} else if (lastChar == 'm' || lastChar == 'M') {
				multiplier = 1024 * 1024;
				value = value.substr(0, value.size() - 1);
			} else if (lastChar == 'g' || lastChar == 'G') {
				multiplier = 1024 * 1024 * 1024;
				value = value.substr(0, value.size() - 1);
			}
			server.clientMaxBody = Utils::toSizeT(value) * multiplier;
			_expectToken(";");
		}
		else if (token == "location") {
			_parseLocation(server);
		}
		else {
			throw std::runtime_error("Unknown directive in server block: '" + token + "'");
		}
	}

	// If no locations, add a default one
	if (server.locations.empty()) {
		LocationConfig defaultLoc;
		defaultLoc.path = "/";
		server.locations.push_back(defaultLoc);
	}

	_servers.push_back(server);
}

/*
** FR: Parse un bloc location { } avec ses directives: root, index,
**     methods, autoindex, return (redirect), upload_path, cgi_ext, cgi_path.
** EN: Parses a location { } block with directives: root, index,
**     methods, autoindex, return (redirect), upload_path, cgi_ext, cgi_path.
*/
void Config::_parseLocation(ServerConfig& server) {
	LocationConfig location;
	location.path = _nextToken();
	_expectToken("{");

	std::string token;
	while (true) {
		token = _nextToken();
		if (token == "}")
			break;
		if (token.empty())
			throw std::runtime_error("Unexpected end of config in location block");

		if (token == "root") {
			location.root = _nextToken();
			_expectToken(";");
		}
		else if (token == "index") {
			location.index = _nextToken();
			_expectToken(";");
		}
		else if (token == "methods" || token == "allow_methods" || token == "limit_except") {
			location.methods.clear();
			while (true) {
				std::string method = _nextToken();
				if (method == ";")
					break;
				location.methods.insert(Utils::toUpper(method));
			}
		}
		else if (token == "autoindex") {
			std::string val = _nextToken();
			location.autoindex = (val == "on");
			_expectToken(";");
		}
		else if (token == "return") {
			std::string codeStr = _nextToken();
			location.redirectCode = Utils::toInt(codeStr);
			location.redirect = _nextToken();
			_expectToken(";");
		}
		else if (token == "upload_path") {
			location.uploadPath = _nextToken();
			_expectToken(";");
		}
		else if (token == "cgi_ext") {
			location.cgiExtension = _nextToken();
			_expectToken(";");
		}
		else if (token == "cgi_path") {
			location.cgiPath = _nextToken();
			_expectToken(";");
		}
		else {
			throw std::runtime_error("Unknown directive in location block: '" + token + "'");
		}
	}

	server.locations.push_back(location);
}

/*
** FR: Valide la config - verifie les ports dupliques (autorise le virtual
**     hosting avec server_names differents), assigne un root "www" par
**     defaut aux locations qui n'en ont pas.
** EN: Validates config - checks duplicate ports (allows virtual hosting
**     with different server_names), assigns default "www" root to
**     locations that don't have one.
*/
void Config::_validateConfig() {
	for (size_t i = 0; i < _servers.size(); ++i) {
		ServerConfig& server = _servers[i];

		// Check for duplicate listen addresses
		for (size_t j = i + 1; j < _servers.size(); ++j) {
			if (_servers[j].host == server.host && _servers[j].port == server.port) {
				// Same host:port with different server_names = virtual hosting (allowed)
				bool hasDifferentNames = false;
				if (!server.serverNames.empty() && !_servers[j].serverNames.empty()) {
					for (size_t k = 0; k < server.serverNames.size(); ++k) {
						for (size_t l = 0; l < _servers[j].serverNames.size(); ++l) {
							if (server.serverNames[k] != _servers[j].serverNames[l])
								hasDifferentNames = true;
						}
					}
				}
				if (!hasDifferentNames) {
					LOG_WARN("Duplicate server on " << server.host << ":" << server.port
							 << " (same server_name). Second block will be ignored.");
				}
			}
		}

		// Ensure all locations have a root if the server doesn't provide a default
		for (size_t j = 0; j < server.locations.size(); ++j) {
			LocationConfig& loc = server.locations[j];
			if (loc.root.empty())
				loc.root = "www";
		}
	}
}
