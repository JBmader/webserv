/* ************************************************************************** */
/*  RESPONSE.HPP                                                              */
/*  FR: Constructeur de reponses HTTP                                         */
/*  EN: HTTP response builder                                                 */
/* ************************************************************************** */

#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "Webserv.hpp"
# include "Config.hpp"
# include "Request.hpp"

class Response {
public:
	Response();
	~Response();

	// FR: Les 4 methodes publiques de construction de reponse:
	// EN: The 4 public response building methods:

	// FR: Construit une reponse normale (GET/POST/DELETE) selon la requete et la config
	// EN: Builds a normal response (GET/POST/DELETE) based on request and config
	void	build(const Request& req, const ServerConfig& server,
				  const LocationConfig& location);
	// FR: Construit une page d'erreur HTTP (404, 500, etc.)
	// EN: Builds an HTTP error page (404, 500, etc.)
	void	buildError(int code, const ServerConfig& server);
	// FR: Construit une reponse de redirection HTTP (301, 302, etc.)
	// EN: Builds an HTTP redirect response (301, 302, etc.)
	void	buildRedirect(int code, const std::string& url);

	const std::string&	getData() const;
	bool				isReady() const;
	size_t				totalSize() const;

	// FR: Injecte la sortie d'un script CGI comme corps de reponse
	// EN: Sets the CGI script output as the response body
	void	setCGIResponse(const std::string& cgiOutput, const ServerConfig& server);

private:
	std::string		_data;
	bool			_ready;

	// FR: Gestionnaires par methode HTTP / EN: Per-method HTTP handlers
	void	_handleGet(const Request& req, const ServerConfig& server,
					   const LocationConfig& location);
	void	_handlePost(const Request& req, const ServerConfig& server,
						const LocationConfig& location);
	void	_handleDelete(const Request& req, const ServerConfig& server,
						  const LocationConfig& location);
	void	_buildResponse(int code, const std::map<std::string, std::string>& headers,
						   const std::string& body);
	// FR: Sert un fichier statique depuis le disque
	// EN: Serves a static file from disk
	void	_serveFile(const std::string& path, const ServerConfig& server);
	// FR: Genere un listing HTML du contenu d'un repertoire
	// EN: Generates an HTML directory listing
	void	_serveAutoindex(const std::string& path, const std::string& uri);
	// FR: Gere l'upload de fichiers (POST multipart)
	// EN: Handles file uploads (POST multipart)
	void	_handleUpload(const Request& req, const LocationConfig& location,
						  const ServerConfig& server);
	std::string	_resolvePath(const Request& req, const LocationConfig& location);
};

#endif
