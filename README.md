*This project has been created as part of the 42 curriculum by anebbou and jmader.*

# Webserv

## Description

Webserv is an HTTP/1.1 server written in C++98, built from the ground up as part of the 42 school curriculum. The project implements the core features of a web server: serving static files, handling file uploads, executing CGI scripts, and managing multiple simultaneous client connections through a non-blocking event-driven architecture powered by `poll()`.

The goal is to understand how the HTTP protocol works under the hood — from parsing raw TCP data into structured requests, to routing URIs, to building and sending well-formed HTTP responses.

### Key features

- **Non-blocking I/O** — single `poll()` event loop handles all clients, listen sockets, and CGI pipes simultaneously.
- **HTTP methods** — GET, POST, DELETE fully supported.
- **Static file serving** — HTML, CSS, JS, images, and more with proper MIME types.
- **File upload** — multipart/form-data and raw body uploads.
- **CGI execution** — Python scripts via `fork()` + `execve()`, with full environment variable setup.
- **Directory listing** — configurable autoindex.
- **HTTP redirections** — 301, 302, etc.
- **Custom error pages** — configurable per status code, with built-in defaults.
- **Multi-port** — a single process listens on multiple ports to serve different content.
- **NGINX-style configuration** — familiar `server { }` / `location { }` block syntax.
- **Timeout handling** — idle client and CGI timeouts to prevent resource leaks.
- **Resilience** — no crashes on bad requests, broken pipes, or out-of-memory conditions.

## Instructions

### Prerequisites

- A C++ compiler supporting C++98 (e.g. `c++`, `g++`, `clang++`)
- `make`
- Python 3 (for CGI scripts)

### Compilation

```bash
make        # Build the project
make clean  # Remove object files
make fclean # Remove object files and binary
make re     # Full rebuild
```

The binary is compiled with `-Wall -Wextra -Werror -std=c++98`.

### Execution

```bash
# With a configuration file
./webserv config/default.conf

# Without arguments (uses config/default.conf by default)
./webserv
```

Then open [http://localhost:8080](http://localhost:8080) in your browser.

### Testing

```bash
# Basic GET
curl -v http://localhost:8080/

# Upload a file
curl -X POST -F "file=@myfile.txt" http://localhost:8080/upload

# Delete a file
curl -X DELETE http://localhost:8080/upload/myfile.txt

# CGI script
curl http://localhost:8080/cgi-bin/hello.py

# POST to CGI
curl -X POST -d "name=42" http://localhost:8080/cgi-bin/form.py

# Test 404
curl -v http://localhost:8080/nonexistent

# Test redirect
curl -v http://localhost:8080/old-page

# Test second server (port 8081)
curl http://localhost:8081/
```

### Configuration

The configuration file uses an NGINX-inspired syntax. Example:

```nginx
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 10M;

    error_page 404 www/errors/404.html;

    location / {
        root www;
        index index.html;
        methods GET POST DELETE;
        autoindex off;
    }

    location /upload {
        root www/upload;
        methods GET POST DELETE;
        autoindex on;
        upload_path www/upload;
    }

    location /cgi-bin {
        root cgi-bin;
        methods GET POST;
        cgi_ext .py;
        cgi_path /usr/bin/python3;
    }
}
```

#### Available directives

| Directive | Context | Description |
|-----------|---------|-------------|
| `listen` | server | `host:port` or `port` to listen on |
| `server_name` | server | Server hostname(s) |
| `client_max_body_size` | server | Max request body size (e.g. `10M`, `1024K`) |
| `error_page` | server | Custom error page: `error_page 404 /path/to/404.html;` |
| `root` | location | Document root directory |
| `index` | location | Default index file |
| `methods` | location | Allowed HTTP methods |
| `autoindex` | location | Enable directory listing (`on`/`off`) |
| `return` | location | HTTP redirect: `return 301 /new-url;` |
| `upload_path` | location | Directory for uploaded files |
| `cgi_ext` | location | CGI file extension (e.g. `.py`) |
| `cgi_path` | location | Path to CGI interpreter |

## Resources

### References

- [RFC 2616 — HTTP/1.1](https://www.rfc-editor.org/rfc/rfc2616) — the main HTTP/1.1 specification.
- [RFC 3875 — CGI/1.1](https://www.rfc-editor.org/rfc/rfc3875) — the Common Gateway Interface specification.
- [NGINX Documentation](https://nginx.org/en/docs/) — used as reference for configuration file syntax and server behavior.
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — socket programming fundamentals.
- [HTTP Made Really Easy](https://www.jmarshall.com/easy/http/) — simplified HTTP overview.

