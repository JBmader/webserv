/* ************************************************************************** */
/*  MAIN.CPP                                                                  */
/*  FR: Point d'entree du serveur - charge la config et lance la boucle       */
/*  EN: Server entry point - loads config and starts the event loop           */
/* ************************************************************************** */

#include "Webserv.hpp"
#include "Config.hpp"
#include "Server.hpp"

/*
** FR: Fonction principale - parse les arguments, charge le fichier de config,
**     cree le serveur et lance la boucle evenementielle. En cas d'erreur,
**     affiche le message et retourne 1.
** EN: Main function - parses arguments, loads config file, creates the server
**     and runs the event loop. On error, prints message and returns 1.
*/
int main(int argc, char **argv) {
	std::string configPath = DEFAULT_CONFIG;

	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
		return 1;
	}
	if (argc == 2)
		configPath = argv[1];

	try {
		Config config;
		config.parse(configPath);
		LOG_INFO("Configuration loaded from " << configPath);

		Server server(config);
		server.run();
	}
	catch (const std::exception& e) {
		LOG_ERROR(e.what());
		return 1;
	}

	return 0;
}
