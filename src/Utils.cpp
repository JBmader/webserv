#include "Utils.hpp"

namespace Utils {

std::string toString(int n) {
	std::ostringstream oss;
	oss << n;
	return oss.str();
}

std::string toString(size_t n) {
	std::ostringstream oss;
	oss << n;
	return oss.str();
}

int toInt(const std::string& s) {
	std::istringstream iss(s);
	int n = 0;
	iss >> n;
	return n;
}

size_t toSizeT(const std::string& s) {
	std::istringstream iss(s);
	size_t n = 0;
	iss >> n;
	return n;
}

std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

std::string toLower(const std::string& s) {
	std::string result = s;
	for (size_t i = 0; i < result.size(); ++i)
		result[i] = std::tolower(static_cast<unsigned char>(result[i]));
	return result;
}

std::string toUpper(const std::string& s) {
	std::string result = s;
	for (size_t i = 0; i < result.size(); ++i)
		result[i] = std::toupper(static_cast<unsigned char>(result[i]));
	return result;
}

bool startsWith(const std::string& s, const std::string& prefix) {
	if (prefix.size() > s.size())
		return false;
	return s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix) {
	if (suffix.size() > s.size())
		return false;
	return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string getMimeType(const std::string& path) {
	std::string ext = getExtension(path);
	if (ext == ".html" || ext == ".htm")	return "text/html";
	if (ext == ".css")						return "text/css";
	if (ext == ".js")						return "application/javascript";
	if (ext == ".json")						return "application/json";
	if (ext == ".xml")						return "application/xml";
	if (ext == ".txt")						return "text/plain";
	if (ext == ".png")						return "image/png";
	if (ext == ".jpg" || ext == ".jpeg")	return "image/jpeg";
	if (ext == ".gif")						return "image/gif";
	if (ext == ".svg")						return "image/svg+xml";
	if (ext == ".ico")						return "image/x-icon";
	if (ext == ".pdf")						return "application/pdf";
	if (ext == ".zip")						return "application/zip";
	if (ext == ".mp3")						return "audio/mpeg";
	if (ext == ".mp4")						return "video/mp4";
	if (ext == ".woff")						return "font/woff";
	if (ext == ".woff2")					return "font/woff2";
	if (ext == ".ttf")						return "font/ttf";
	return "application/octet-stream";
}

std::string getExtension(const std::string& path) {
	size_t dot = path.rfind('.');
	if (dot == std::string::npos || dot == path.size() - 1)
		return "";
	// Make sure dot is in filename, not in directory path
	size_t slash = path.rfind('/');
	if (slash != std::string::npos && dot < slash)
		return "";
	return path.substr(dot);
}

std::string getStatusText(int code) {
	switch (code) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 307: return "Temporary Redirect";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		default:  return "Unknown";
	}
}

std::string getDate() {
	time_t now = time(NULL);
	struct tm* gmt = gmtime(&now);
	char buf[128];
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
	return std::string(buf);
}

std::string urlDecode(const std::string& s) {
	std::string result;
	result.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '%' && i + 2 < s.size()) {
			std::string hex = s.substr(i + 1, 2);
			char c = static_cast<char>(strtol(hex.c_str(), NULL, 16));
			if (c == '\0')
				continue; // reject null bytes
			result += c;
			i += 2;
		} else if (s[i] == '+') {
			result += ' ';
		} else {
			result += s[i];
		}
	}
	return result;
}

std::string readFile(const std::string& path) {
	std::ifstream ifs(path.c_str(), std::ios::binary);
	if (!ifs.is_open())
		return "";
	std::ostringstream oss;
	oss << ifs.rdbuf();
	return oss.str();
}

bool fileExists(const std::string& path) {
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

bool isDirectory(const std::string& path) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

bool isRegularFile(const std::string& path) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISREG(st.st_mode);
}

std::string sanitizePath(const std::string& path) {
	// Remove consecutive slashes, resolve . and .. to prevent path traversal
	std::vector<std::string> parts;
	std::istringstream iss(path);
	std::string part;
	while (std::getline(iss, part, '/')) {
		if (part.empty() || part == ".")
			continue;
		if (part == "..") {
			if (!parts.empty())
				parts.pop_back();
		} else {
			parts.push_back(part);
		}
	}
	std::string result = "/";
	for (size_t i = 0; i < parts.size(); ++i) {
		result += parts[i];
		if (i + 1 < parts.size())
			result += "/";
	}
	return result;
}

std::string joinPath(const std::string& a, const std::string& b) {
	if (a.empty())
		return b;
	if (b.empty())
		return a;
	if (a[a.size() - 1] == '/' && b[0] == '/')
		return a + b.substr(1);
	if (a[a.size() - 1] != '/' && b[0] != '/')
		return a + "/" + b;
	return a + b;
}

std::string defaultErrorPage(int code) {
	std::string text = getStatusText(code);
	std::string codeStr = toString(code);
	return "<!DOCTYPE html>\n<html>\n<head><title>" + codeStr + " " + text +
		   "</title></head>\n<body>\n<center><h1>" + codeStr + " " + text +
		   "</h1></center>\n<hr>\n<center>" + SERVER_NAME + "</center>\n</body>\n</html>\n";
}

std::string htmlEscape(const std::string& s) {
	std::string result;
	result.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		switch (s[i]) {
			case '&':  result += "&amp;";  break;
			case '<':  result += "&lt;";   break;
			case '>':  result += "&gt;";   break;
			case '"':  result += "&quot;";  break;
			case '\'': result += "&#39;";  break;
			default:   result += s[i];     break;
		}
	}
	return result;
}

std::vector<std::string> split(const std::string& s, char delim) {
	std::vector<std::string> tokens;
	std::istringstream iss(s);
	std::string token;
	while (std::getline(iss, token, delim)) {
		if (!token.empty())
			tokens.push_back(token);
	}
	return tokens;
}

} // namespace Utils
