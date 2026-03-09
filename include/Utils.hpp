#ifndef UTILS_HPP
# define UTILS_HPP

# include "Webserv.hpp"

namespace Utils {

std::string	toString(int n);
std::string	toString(size_t n);
int			toInt(const std::string& s);
size_t		toSizeT(const std::string& s);
std::string	trim(const std::string& s);
std::string	toLower(const std::string& s);
std::string	toUpper(const std::string& s);
bool		startsWith(const std::string& s, const std::string& prefix);
bool		endsWith(const std::string& s, const std::string& suffix);
std::string	getMimeType(const std::string& path);
std::string	getExtension(const std::string& path);
std::string	getStatusText(int code);
std::string	getDate();
std::string	urlDecode(const std::string& s);
std::string	readFile(const std::string& path);
bool		fileExists(const std::string& path);
bool		isDirectory(const std::string& path);
bool		isRegularFile(const std::string& path);
std::string	sanitizePath(const std::string& path);
std::string	joinPath(const std::string& a, const std::string& b);
std::string	defaultErrorPage(int code);
std::vector<std::string>	split(const std::string& s, char delim);

}

#endif
