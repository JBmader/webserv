/* ************************************************************************** */
/*  WEBSERV.HPP                                                               */
/*  FR: En-tete global - includes systeme, constantes, macros de log          */
/*  EN: Global header - system includes, constants, log macros                */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
# define WEBSERV_HPP

// FR: En-tetes C++ standard / EN: Standard C++ headers
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

// FR: En-tetes C / Systeme / EN: C / System headers
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

// ─── Constants / Constantes ──────────────────────────────────────────────────

// FR: Taille du tampon de lecture (4096 octets = 4KB)
// EN: Read buffer size (4096 bytes = 4KB)
# define BUFFER_SIZE		4096

// FR: Taille max des headers HTTP pour prevenir les attaques (8KB)
// EN: Max HTTP header size to prevent attacks (8KB)
# define MAX_HEADER_SIZE	8192

// FR: Longueur max de l'URI
// EN: Max URI length
# define MAX_URI_LENGTH		4096

// FR: Secondes d'inactivite avant deconnexion du client
// EN: Idle seconds before client disconnect
# define CLIENT_TIMEOUT		60

// FR: Secondes max pour l'execution d'un script CGI
// EN: Max seconds for CGI script execution
# define CGI_TIMEOUT		10

# define DEFAULT_CONFIG		"config/default.conf"
# define SERVER_NAME		"Webserv/1.0"

// ─── Log helpers / Macros de journalisation ─────────────────────────────────
// FR: Macros de journalisation vers stdout (INFO) et stderr (ERROR, WARN)
// EN: Logging macros to stdout (INFO) and stderr (ERROR, WARN)

# define LOG_INFO(msg)	std::cout << "[INFO]  " << msg << std::endl
# define LOG_ERROR(msg)	std::cerr << "[ERROR] " << msg << std::endl
# define LOG_WARN(msg)	std::cerr << "[WARN]  " << msg << std::endl

#endif
