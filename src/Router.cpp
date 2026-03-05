/* ************************************************************************** */
/*  ROUTER.CPP                                                                */
/*  FR: Routeur de requetes - associe URI a server/location configs           */
/*  EN: Request router - matches URIs to server/location configs              */
/* ************************************************************************** */

#include "Router.hpp"
#include "Utils.hpp"

namespace Router {

/*
** FR: Delegue a Config::findServer pour trouver le serveur par host et port
** EN: Delegates to Config::findServer to find server by host and port
*/
const ServerConfig* matchServer(const Config& config, const std::string& host, int port) {
	return config.findServer(host, port);
}

/*
** FR: Delegue a ServerConfig::findLocation (longest-prefix match)
** EN: Delegates to ServerConfig::findLocation (longest-prefix match)
*/
const LocationConfig* matchLocation(const ServerConfig& server, const std::string& uri) {
	return server.findLocation(uri);
}

/*
** FR: Verifie si la methode HTTP est dans la liste des methodes autorisees de la location
** EN: Checks if HTTP method is in the location's allowed methods set
*/
bool isMethodAllowed(const LocationConfig& location, const std::string& method) {
	return location.methods.find(method) != location.methods.end();
}

/*
** FR: Verifie si le chemin se termine par l'extension CGI configuree (ex: .py)
** EN: Checks if path ends with configured CGI extension (e.g. .py)
*/
bool isCGI(const LocationConfig& location, const std::string& path) {
	if (location.cgiExtension.empty())
		return false;
	return Utils::endsWith(path, location.cgiExtension);
}

/*
** FR: Resout le chemin complet du fichier: retire le prefixe de la location de l'URI,
**     puis joint avec le root. Exemple: URI=/cgi-bin/hello.py, location.path=/cgi-bin,
**     root=cgi-bin -> cgi-bin/hello.py
** EN: Resolves full file path: strips location prefix from URI, then joins with root.
**     Example: URI=/cgi-bin/hello.py, location.path=/cgi-bin, root=cgi-bin -> cgi-bin/hello.py
*/
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
