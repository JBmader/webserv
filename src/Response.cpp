#include "Response.hpp"
#include "Router.hpp"
#include "Utils.hpp"

Response::Response() : _ready(false) {}
Response::~Response() {}

const std::string& Response::getData() const { return _data; }
bool Response::isReady() const { return _ready; }
size_t Response::totalSize() const { return _data.size(); }

void Response::_buildResponse(int code, const std::map<std::string, std::string>& headers,
							  const std::string& body) {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << code << " " << Utils::getStatusText(code) << "\r\n";

	// Always add Date and Server
	oss << "Date: " << Utils::getDate() << "\r\n";
	oss << "Server: " << SERVER_NAME << "\r\n";

	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		 it != headers.end(); ++it) {
		oss << it->first << ": " << it->second << "\r\n";
	}

	if (headers.find("Content-Length") == headers.end()) {
		oss << "Content-Length: " << body.size() << "\r\n";
	}
	if (headers.find("Connection") == headers.end()) {
		oss << "Connection: close\r\n";
	}

	oss << "\r\n";
	oss << body;
	_data = oss.str();
	_ready = true;
}

void Response::buildError(int code, const ServerConfig& server) {
	// Check for custom error page
	std::map<int, std::string>::const_iterator it = server.errorPages.find(code);
	std::string body;
	std::string contentType = "text/html";

	if (it != server.errorPages.end()) {
		body = Utils::readFile(it->second);
		if (body.empty())
			body = Utils::defaultErrorPage(code);
	} else {
		body = Utils::defaultErrorPage(code);
	}

	std::map<std::string, std::string> headers;
	headers["Content-Type"] = contentType;
	_buildResponse(code, headers, body);
}

void Response::buildRedirect(int code, const std::string& url) {
	std::map<std::string, std::string> headers;
	headers["Location"] = url;
	headers["Content-Type"] = "text/html";
	std::string body = "<html><body><h1>" + Utils::toString(code) + " " +
					   Utils::getStatusText(code) + "</h1><p>Redirecting to <a href=\"" +
					   url + "\">" + url + "</a></p></body></html>";
	_buildResponse(code, headers, body);
}

void Response::build(const Request& req, const ServerConfig& server,
					 const LocationConfig& location) {
	// Check for redirect
	if (location.redirectCode > 0 && !location.redirect.empty()) {
		buildRedirect(location.redirectCode, location.redirect);
		return;
	}

	// Check method
	if (!Router::isMethodAllowed(location, req.getMethod())) {
		// Build 405 with Allow header
		std::string allow;
		for (std::set<std::string>::const_iterator it = location.methods.begin();
			 it != location.methods.end(); ++it) {
			if (!allow.empty())
				allow += ", ";
			allow += *it;
		}
		std::map<std::string, std::string> headers;
		headers["Content-Type"] = "text/html";
		headers["Allow"] = allow;
		_buildResponse(405, headers, Utils::defaultErrorPage(405));
		return;
	}

	if (req.getMethod() == "GET")
		_handleGet(req, server, location);
	else if (req.getMethod() == "POST")
		_handlePost(req, server, location);
	else if (req.getMethod() == "DELETE")
		_handleDelete(req, server, location);
	else
		buildError(501, server);
}

std::string Response::_resolvePath(const Request& req, const LocationConfig& location) {
	return Router::resolvePath(req.getPath(), location);
}

void Response::_handleGet(const Request& req, const ServerConfig& server,
						  const LocationConfig& location) {
	std::string path = _resolvePath(req, location);

	if (Utils::isDirectory(path)) {
		// Try index file
		if (!location.index.empty()) {
			std::string indexPath = Utils::joinPath(path, location.index);
			if (Utils::isRegularFile(indexPath)) {
				_serveFile(indexPath, server);
				return;
			}
		}
		// Autoindex
		if (location.autoindex) {
			_serveAutoindex(path, req.getPath());
			return;
		}
		buildError(403, server);
		return;
	}

	if (Utils::isRegularFile(path)) {
		_serveFile(path, server);
		return;
	}

	buildError(404, server);
}

void Response::_serveFile(const std::string& path, const ServerConfig& server) {
	if (!Utils::fileExists(path)) {
		buildError(404, server);
		return;
	}
	if (access(path.c_str(), R_OK) != 0) {
		buildError(403, server);
		return;
	}
	std::string body = Utils::readFile(path);

	std::map<std::string, std::string> headers;
	headers["Content-Type"] = Utils::getMimeType(path);
	_buildResponse(200, headers, body);
}

void Response::_serveAutoindex(const std::string& path, const std::string& uri) {
	DIR* dir = opendir(path.c_str());
	if (!dir) {
		std::map<std::string, std::string> headers;
		headers["Content-Type"] = "text/html";
		_buildResponse(403, headers, Utils::defaultErrorPage(403));
		return;
	}

	std::string safeUri = Utils::htmlEscape(uri);
	std::string body = "<!DOCTYPE html>\n<html>\n<head><title>Index of " + safeUri +
					   "</title></head>\n<body>\n<h1>Index of " + safeUri + "</h1>\n<hr>\n<pre>\n";

	// Ensure URI ends with /
	std::string baseUri = uri;
	if (baseUri.empty() || baseUri[baseUri.size() - 1] != '/')
		baseUri += "/";

	struct dirent* ent;
	std::vector<std::string> entries;
	while ((ent = readdir(dir)) != NULL) {
		std::string name = ent->d_name;
		if (name == ".")
			continue;
		entries.push_back(name);
	}
	closedir(dir);

	std::sort(entries.begin(), entries.end());

	for (size_t i = 0; i < entries.size(); ++i) {
		std::string fullPath = Utils::joinPath(path, entries[i]);
		std::string display = Utils::htmlEscape(entries[i]);
		if (Utils::isDirectory(fullPath))
			display += "/";
		body += "<a href=\"" + baseUri + entries[i];
		if (Utils::isDirectory(fullPath))
			body += "/";
		body += "\">" + display + "</a>\n";
	}

	body += "</pre>\n<hr>\n</body>\n</html>\n";

	std::map<std::string, std::string> headers;
	headers["Content-Type"] = "text/html";
	_buildResponse(200, headers, body);
}

void Response::_handlePost(const Request& req, const ServerConfig& server,
						   const LocationConfig& location) {
	// Check if it's a CGI request — but CGI is handled by the Client, not here
	// If we reach here with a non-CGI POST, handle upload
	if (!location.uploadPath.empty()) {
		_handleUpload(req, location, server);
		return;
	}

	// Non-CGI, non-upload POST: just serve the resource like GET
	_handleGet(req, server, location);
}

void Response::_handleUpload(const Request& req, const LocationConfig& location,
							 const ServerConfig& server) {
	std::string contentType = req.getHeader("Content-Type");
	const std::string& body = req.getBody();

	if (body.empty()) {
		buildError(400, server);
		return;
	}

	std::string uploadDir = location.uploadPath;

	// Create upload directory if it doesn't exist
	if (!Utils::isDirectory(uploadDir)) {
		mkdir(uploadDir.c_str(), 0755);
	}

	std::string filename;
	std::string fileContent;

	// Check for multipart/form-data
	if (contentType.find("multipart/form-data") != std::string::npos) {
		// Extract boundary
		size_t bpos = contentType.find("boundary=");
		if (bpos == std::string::npos) {
			buildError(400, server);
			return;
		}
		std::string boundary = "--" + contentType.substr(bpos + 9);

		// Find the first file part
		size_t partStart = body.find(boundary);
		if (partStart == std::string::npos) {
			buildError(400, server);
			return;
		}
		partStart += boundary.size() + 2; // skip \r\n

		// Find Content-Disposition for filename
		size_t dispPos = body.find("Content-Disposition:", partStart);
		if (dispPos == std::string::npos) {
			buildError(400, server);
			return;
		}
		size_t fnPos = body.find("filename=\"", dispPos);
		if (fnPos == std::string::npos) {
			buildError(400, server);
			return;
		}
		fnPos += 10;
		size_t fnEnd = body.find("\"", fnPos);
		filename = body.substr(fnPos, fnEnd - fnPos);

		// Find body start (after double \r\n)
		size_t bodyStart = body.find("\r\n\r\n", dispPos);
		if (bodyStart == std::string::npos) {
			buildError(400, server);
			return;
		}
		bodyStart += 4;

		// Find body end (next boundary)
		size_t bodyEnd = body.find(boundary, bodyStart);
		if (bodyEnd == std::string::npos) {
			bodyEnd = body.size();
		} else {
			bodyEnd -= 2; // remove trailing \r\n before boundary
		}

		fileContent = body.substr(bodyStart, bodyEnd - bodyStart);
	} else {
		// Raw body upload - use a timestamp as filename
		filename = "upload_" + Utils::toString(static_cast<int>(time(NULL)));
		fileContent = body;
	}

	if (filename.empty()) {
		filename = "upload_" + Utils::toString(static_cast<int>(time(NULL)));
	}

	// Sanitize filename (remove directory separators)
	for (size_t i = 0; i < filename.size(); ++i) {
		if (filename[i] == '/' || filename[i] == '\\')
			filename[i] = '_';
	}

	std::string filePath = Utils::joinPath(uploadDir, filename);

	std::ofstream ofs(filePath.c_str(), std::ios::binary);
	if (!ofs.is_open()) {
		buildError(500, server);
		return;
	}
	ofs.write(fileContent.c_str(), fileContent.size());
	ofs.close();

	std::map<std::string, std::string> headers;
	headers["Content-Type"] = "text/html";
	headers["Location"] = "/" + filename;
	std::string responseBody = "<html><body><h1>201 Created</h1><p>File '" + filename +
							   "' uploaded successfully.</p></body></html>";
	_buildResponse(201, headers, responseBody);
}

void Response::_handleDelete(const Request& req, const ServerConfig& server,
							 const LocationConfig& location) {
	std::string path = _resolvePath(req, location);

	if (!Utils::fileExists(path)) {
		buildError(404, server);
		return;
	}

	if (Utils::isDirectory(path)) {
		buildError(403, server);
		return;
	}

	if (std::remove(path.c_str()) != 0) {
		buildError(500, server);
		return;
	}

	std::map<std::string, std::string> headers;
	headers["Content-Type"] = "text/html";
	std::string body = "<html><body><h1>200 OK</h1><p>File deleted.</p></body></html>";
	_buildResponse(200, headers, body);
}

void Response::setCGIResponse(const std::string& cgiOutput, const ServerConfig& server) {
	// CGI output format: headers\r\n\r\nbody (or \n\n)
	std::string lineEnd = "\r\n";
	size_t headerEnd = cgiOutput.find("\r\n\r\n");
	if (headerEnd == std::string::npos) {
		lineEnd = "\n";
		headerEnd = cgiOutput.find("\n\n");
		if (headerEnd == std::string::npos) {
			buildError(502, server);
			return;
		}
	}

	std::string cgiHeaders = cgiOutput.substr(0, headerEnd);
	std::string cgiBody = cgiOutput.substr(headerEnd + (lineEnd.size() * 2));

	// Parse CGI headers
	int statusCode = 200;
	std::map<std::string, std::string> headers;
	std::istringstream iss(cgiHeaders);
	std::string line;
	while (std::getline(iss, line)) {
		// Remove trailing \r if present
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::string key = Utils::trim(line.substr(0, colon));
		std::string value = Utils::trim(line.substr(colon + 1));
		if (Utils::toLower(key) == "status") {
			statusCode = Utils::toInt(value);
		} else {
			headers[key] = value;
		}
	}

	if (headers.find("Content-Type") == headers.end())
		headers["Content-Type"] = "text/html";

	_buildResponse(statusCode, headers, cgiBody);
	(void)server;
}
