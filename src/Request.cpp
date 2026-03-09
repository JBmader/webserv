#include "Request.hpp"
#include "Utils.hpp"

Request::Request()
	: _state(PARSE_REQUEST_LINE), _contentLength(0), _chunked(false),
	  _maxBodySize(1048576), _errorCode(0) {
}

Request::~Request() {}

void Request::reset() {
	_state = PARSE_REQUEST_LINE;
	_raw.clear();
	_method.clear();
	_uri.clear();
	_path.clear();
	_query.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_contentLength = 0;
	_chunked = false;
	_errorCode = 0;
}

void Request::setMaxBodySize(size_t size) {
	_maxBodySize = size;
}

bool Request::feed(const std::string& data) {
	if (_state == PARSE_COMPLETE || _state == PARSE_ERROR)
		return true;

	_raw += data;

	if (_state == PARSE_REQUEST_LINE) {
		if (!_parseRequestLine())
			return false;
	}
	if (_state == PARSE_HEADERS) {
		if (!_parseHeaders())
			return false;
	}
	if (_state == PARSE_BODY) {
		if (!_parseBody())
			return false;
	}
	if (_state == PARSE_CHUNKED) {
		if (!_parseChunked())
			return false;
	}

	return (_state == PARSE_COMPLETE || _state == PARSE_ERROR);
}

bool Request::_parseRequestLine() {
	size_t pos = _raw.find("\r\n");
	if (pos == std::string::npos) {
		if (_raw.size() > MAX_URI_LENGTH + 256) {
			_errorCode = 414;
			_state = PARSE_ERROR;
			return true;
		}
		return false;
	}

	std::string line = _raw.substr(0, pos);
	_raw.erase(0, pos + 2);

	// Parse: METHOD SP URI SP VERSION
	size_t sp1 = line.find(' ');
	if (sp1 == std::string::npos) {
		_errorCode = 400;
		_state = PARSE_ERROR;
		return true;
	}
	_method = line.substr(0, sp1);

	size_t sp2 = line.find(' ', sp1 + 1);
	if (sp2 == std::string::npos) {
		_errorCode = 400;
		_state = PARSE_ERROR;
		return true;
	}
	_uri = line.substr(sp1 + 1, sp2 - sp1 - 1);
	_version = line.substr(sp2 + 1);

	// Validate method
	if (_method != "GET" && _method != "POST" && _method != "DELETE" &&
		_method != "HEAD" && _method != "PUT") {
		_errorCode = 501;
		_state = PARSE_ERROR;
		return true;
	}

	// Validate version
	if (_version != "HTTP/1.0" && _version != "HTTP/1.1") {
		_errorCode = 505;
		_state = PARSE_ERROR;
		return true;
	}

	// Validate URI length
	if (_uri.size() > MAX_URI_LENGTH) {
		_errorCode = 414;
		_state = PARSE_ERROR;
		return true;
	}

	_parseUri();
	_state = PARSE_HEADERS;
	return true;
}

void Request::_parseUri() {
	size_t qpos = _uri.find('?');
	if (qpos != std::string::npos) {
		_path = Utils::urlDecode(_uri.substr(0, qpos));
		_query = _uri.substr(qpos + 1);
	} else {
		_path = Utils::urlDecode(_uri);
		_query = "";
	}
	_path = Utils::sanitizePath(_path);
}

bool Request::_parseHeaders() {
	while (true) {
		size_t pos = _raw.find("\r\n");
		if (pos == std::string::npos) {
			if (_raw.size() > MAX_HEADER_SIZE) {
				_errorCode = 400;
				_state = PARSE_ERROR;
				return true;
			}
			return false;
		}

		// Empty line = end of headers
		if (pos == 0) {
			_raw.erase(0, 2);

			// Determine body handling
			std::string te = Utils::toLower(getHeader("Transfer-Encoding"));
			if (te.find("chunked") != std::string::npos) {
				_chunked = true;
				_state = PARSE_CHUNKED;
			} else {
				std::string cl = getHeader("Content-Length");
				if (!cl.empty()) {
					_contentLength = Utils::toSizeT(cl);
					if (_contentLength > _maxBodySize) {
						_errorCode = 413;
						_state = PARSE_ERROR;
						return true;
					}
					if (_contentLength == 0) {
						_state = PARSE_COMPLETE;
					} else {
						_state = PARSE_BODY;
					}
				} else {
					// No body
					_state = PARSE_COMPLETE;
				}
			}
			return true;
		}

		std::string line = _raw.substr(0, pos);
		_raw.erase(0, pos + 2);

		size_t colon = line.find(':');
		if (colon == std::string::npos) {
			_errorCode = 400;
			_state = PARSE_ERROR;
			return true;
		}

		std::string key = Utils::toLower(Utils::trim(line.substr(0, colon)));
		std::string value = Utils::trim(line.substr(colon + 1));
		_headers[key] = value;
	}
}

bool Request::_parseBody() {
	if (_raw.size() >= _contentLength) {
		_body = _raw.substr(0, _contentLength);
		_raw.erase(0, _contentLength);
		_state = PARSE_COMPLETE;
		return true;
	}
	// Check for body too large during incremental reception
	if (_raw.size() > _maxBodySize) {
		_errorCode = 413;
		_state = PARSE_ERROR;
		return true;
	}
	return false;
}

bool Request::_parseChunked() {
	while (true) {
		size_t pos = _raw.find("\r\n");
		if (pos == std::string::npos)
			return false;

		// Parse chunk size (hex)
		std::string sizeStr = _raw.substr(0, pos);
		unsigned long chunkSize = strtoul(sizeStr.c_str(), NULL, 16);

		if (chunkSize == 0) {
			// Last chunk - skip trailing \r\n
			_raw.erase(0, pos + 2);
			// Skip optional trailer headers and final \r\n
			size_t endPos = _raw.find("\r\n");
			if (endPos != std::string::npos)
				_raw.erase(0, endPos + 2);
			_state = PARSE_COMPLETE;
			return true;
		}

		// Check if we have the full chunk data + \r\n
		if (_raw.size() < pos + 2 + chunkSize + 2)
			return false;

		// Extract chunk data
		std::string chunk = _raw.substr(pos + 2, chunkSize);
		_body += chunk;
		_raw.erase(0, pos + 2 + chunkSize + 2);

		// Check body size limit
		if (_body.size() > _maxBodySize) {
			_errorCode = 413;
			_state = PARSE_ERROR;
			return true;
		}
	}
}

// ─── Accessors ───────────────────────────────────────────────────────────────

const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getUri() const { return _uri; }
const std::string& Request::getPath() const { return _path; }
const std::string& Request::getQuery() const { return _query; }
const std::string& Request::getVersion() const { return _version; }
const std::map<std::string, std::string>& Request::getHeaders() const { return _headers; }
const std::string& Request::getBody() const { return _body; }
Request::ParseState Request::getState() const { return _state; }
int Request::getErrorCode() const { return _errorCode; }

std::string Request::getHeader(const std::string& key) const {
	std::string lkey = Utils::toLower(key);
	std::map<std::string, std::string>::const_iterator it = _headers.find(lkey);
	if (it != _headers.end())
		return it->second;
	return "";
}
