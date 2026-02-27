/* ************************************************************************** */
/*  REQUEST.HPP                                                               */
/*  FR: Parseur de requetes HTTP incremental                                  */
/*  EN: Incremental HTTP request parser                                       */
/* ************************************************************************** */

#ifndef REQUEST_HPP
# define REQUEST_HPP

# include "Webserv.hpp"

class Request {
public:
	/*
	** FR: Machine a etats du parsing:
	**     REQUEST_LINE -> HEADERS -> BODY ou CHUNKED -> COMPLETE ou ERROR
	** EN: Parsing state machine:
	**     REQUEST_LINE -> HEADERS -> BODY or CHUNKED -> COMPLETE or ERROR
	*/
	enum ParseState {
		PARSE_REQUEST_LINE,
		PARSE_HEADERS,
		PARSE_BODY,
		PARSE_CHUNKED,
		PARSE_COMPLETE,
		PARSE_ERROR
	};

	Request();
	~Request();

	// FR: Methode principale - alimente le parseur avec des donnees brutes du socket.
	//     Retourne true quand le parsing est termine ou en erreur.
	// EN: Main method - feeds parser with raw socket data.
	//     Returns true when parsing is complete or on error.
	bool	feed(const std::string& data);

	// FR: Accesseurs / EN: Accessors
	const std::string&						getMethod() const;
	const std::string&						getUri() const;
	const std::string&						getPath() const;
	const std::string&						getQuery() const;
	const std::string&						getVersion() const;
	const std::map<std::string, std::string>&	getHeaders() const;
	std::string								getHeader(const std::string& key) const;
	const std::string&						getBody() const;
	ParseState								getState() const;
	int										getErrorCode() const;

	void	setMaxBodySize(size_t size);
	void	reset();

private:
	ParseState						_state;
	// FR: Buffer brut non encore parse / EN: Raw unparsed buffer
	std::string						_raw;
	std::string						_method;
	std::string						_uri;
	std::string						_path;
	std::string						_query;
	std::string						_version;
	std::map<std::string, std::string>	_headers;
	std::string						_body;
	// FR: Taille du body attendue via Content-Length
	// EN: Expected body size from Content-Length
	size_t							_contentLength;
	// FR: true si Transfer-Encoding: chunked
	// EN: true if Transfer-Encoding: chunked
	bool							_chunked;
	// FR: Taille max du body (depuis client_max_body_size)
	// EN: Max body size (from client_max_body_size)
	size_t							_maxBodySize;
	// FR: Code d'erreur HTTP si parsing echoue (400, 413, 414, 501)
	// EN: HTTP error code if parsing fails (400, 413, 414, 501)
	int								_errorCode;

	bool	_parseRequestLine();
	bool	_parseHeaders();
	bool	_parseBody();
	bool	_parseChunked();
	void	_parseUri();
};

#endif
