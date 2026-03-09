#ifndef WEBSERV_HPP
# define WEBSERV_HPP

// C++ headers
# include <iostream>
# include <string>
# include <sstream>
# include <fstream>
# include <vector>
# include <map>
# include <set>
# include <algorithm>
# include <cstring>
# include <cstdlib>
# include <cstdio>
# include <cerrno>
# include <ctime>
# include <climits>

// C / System headers
# include <unistd.h>
# include <fcntl.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/wait.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <poll.h>
# include <dirent.h>

// ─── Constants ───────────────────────────────────────────────────────────────

# define BUFFER_SIZE		4096
# define MAX_HEADER_SIZE	8192
# define MAX_URI_LENGTH		4096
# define CLIENT_TIMEOUT		60
# define CGI_TIMEOUT		10
# define DEFAULT_CONFIG		"config/default.conf"
# define SERVER_NAME		"Webserv/1.0"

// ─── Log helpers ─────────────────────────────────────────────────────────────

# define LOG_INFO(msg)	std::cout << "[INFO]  " << msg << std::endl
# define LOG_ERROR(msg)	std::cerr << "[ERROR] " << msg << std::endl
# define LOG_WARN(msg)	std::cerr << "[WARN]  " << msg << std::endl

#endif
