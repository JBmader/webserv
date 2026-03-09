#include "Webserv.hpp"
#include "Config.hpp"
#include "Server.hpp"

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
