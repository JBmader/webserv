#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp"

namespace Router {

// Find the matching server config for a request
const ServerConfig*		matchServer(const Config& config, const std::string& host, int port);

// Find the matching location in a server config for a URI
const LocationConfig*	matchLocation(const ServerConfig& server, const std::string& uri);

// Check if the method is allowed for the location
bool					isMethodAllowed(const LocationConfig& location, const std::string& method);

// Check if the request targets a CGI script
bool					isCGI(const LocationConfig& location, const std::string& path);

// Resolve the filesystem path from URI + location config
std::string				resolvePath(const std::string& uri, const LocationConfig& location);

}

#endif
