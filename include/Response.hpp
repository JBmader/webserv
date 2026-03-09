#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp"

class Response {
public:
	Response();
	~Response();

	void	build(const Request& req, const ServerConfig& server,
				  const LocationConfig& location);
	void	buildError(int code, const ServerConfig& server);
	void	buildRedirect(int code, const std::string& url);

	const std::string&	getData() const;
	bool				isReady() const;
	size_t				totalSize() const;

	// For CGI: set body from CGI output
	void	setCGIResponse(const std::string& cgiOutput, const ServerConfig& server);

private:
	std::string		_data;
	bool			_ready;

	void	_handleGet(const Request& req, const ServerConfig& server,
					   const LocationConfig& location);
	void	_handlePost(const Request& req, const ServerConfig& server,
						const LocationConfig& location);
	void	_handleDelete(const Request& req, const ServerConfig& server,
						  const LocationConfig& location);
	void	_buildResponse(int code, const std::map<std::string, std::string>& headers,
						   const std::string& body);
	void	_serveFile(const std::string& path, const ServerConfig& server);
	void	_serveAutoindex(const std::string& path, const std::string& uri);
	void	_handleUpload(const Request& req, const LocationConfig& location,
						  const ServerConfig& server);
	std::string	_resolvePath(const Request& req, const LocationConfig& location);
};

#endif
