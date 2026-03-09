#include "Router.hpp"
#include "Utils.hpp"

namespace Router {

const ServerConfig* matchServer(const Config& config, const std::string& host, int port) {
	return config.findServer(host, port);
}

const LocationConfig* matchLocation(const ServerConfig& server, const std::string& uri) {
	return server.findLocation(uri);
}

bool isMethodAllowed(const LocationConfig& location, const std::string& method) {
	return location.methods.find(method) != location.methods.end();
}

bool isCGI(const LocationConfig& location, const std::string& path) {
	if (location.cgiExtension.empty())
		return false;
	return Utils::endsWith(path, location.cgiExtension);
}

std::string resolvePath(const std::string& uri, const LocationConfig& location) {
	// Remove the location prefix from the URI, then append to root
	std::string relative = uri;
	if (Utils::startsWith(relative, location.path)) {
		relative = relative.substr(location.path.size());
	}
	// Ensure relative doesn't start with /
	while (!relative.empty() && relative[0] == '/')
		relative = relative.substr(1);

	return Utils::joinPath(location.root, relative);
}

} // namespace Router
