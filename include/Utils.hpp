/* ************************************************************************** */
/*  UTILS.HPP                                                                 */
/*  FR: Fonctions utilitaires - conversions, fichiers, securite               */
/*  EN: Utility functions - conversions, files, security                      */
/* ************************************************************************** */

#ifndef UTILS_HPP
# define UTILS_HPP

# include "Webserv.hpp"

namespace Utils {

// FR: Conversions de types / EN: Type conversions
std::string	toString(int n);
std::string	toString(size_t n);
int			toInt(const std::string& s);
size_t		toSizeT(const std::string& s);

// FR: Manipulation de chaines / EN: String manipulation
std::string	trim(const std::string& s);
std::string	toLower(const std::string& s);
std::string	toUpper(const std::string& s);
bool		startsWith(const std::string& s, const std::string& prefix);
bool		endsWith(const std::string& s, const std::string& suffix);
std::vector<std::string>	split(const std::string& s, char delim);

// FR: HTTP et MIME / EN: HTTP and MIME
std::string	getMimeType(const std::string& path);
std::string	getExtension(const std::string& path);
std::string	getStatusText(int code);
std::string	getDate();
std::string	defaultErrorPage(int code);

// FR: Encodage et securite / EN: Encoding and security
std::string	urlDecode(const std::string& s);
// FR: Nettoie le chemin pour empecher les traversees de repertoires (../)
// EN: Sanitizes path to prevent directory traversal attacks (../)
std::string	sanitizePath(const std::string& path);
std::string	htmlEscape(const std::string& s);

// FR: Systeme de fichiers / EN: Filesystem operations
std::string	readFile(const std::string& path);
bool		fileExists(const std::string& path);
bool		isDirectory(const std::string& path);
bool		isRegularFile(const std::string& path);
std::string	joinPath(const std::string& a, const std::string& b);

}

#endif
