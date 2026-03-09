#include "Config.hpp"
#include "Utils.hpp"

// ─── LocationConfig ──────────────────────────────────────────────────────────

LocationConfig::LocationConfig()
	: path("/"), root(""), index("index.html"), autoindex(false),
	  redirectCode(0), cgiExtension(""), cgiPath("") {
	methods.insert("GET");
	methods.insert("POST");
	methods.insert("DELETE");
}

// ─── ServerConfig ────────────────────────────────────────────────────────────

ServerConfig::ServerConfig()
	: host("0.0.0.0"), port(8080), clientMaxBody(1048576) {
}

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

void Config::_skipWhitespace() {
	while (_pos < _content.size() &&
		   (_content[_pos] == ' ' || _content[_pos] == '\t' ||
			_content[_pos] == '\r' || _content[_pos] == '\n'))
		++_pos;
}

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

std::string Config::_expectToken(const std::string& expected) {
	std::string token = _nextToken();
	if (token != expected)
		throw std::runtime_error("Expected '" + expected + "', got '" + token + "'");
	return token;
}

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
