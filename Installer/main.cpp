#include <expected>
#include <print>
#include <iostream>
#define BOOST_DLL_USE_STD_FS
#include <boost/dll/runtime_symbol_info.hpp>
import Mediaboard.Installer;

const std::string current_version = "0.3";

using namespace Mediaboard;

int main(int argc, char* argv[]) {
	try {
		std::filesystem::path program_root = boost::dll::program_location().parent_path().parent_path();
		InstallLocation install_location("Fuze Mediaboard", program_root, current_version);
		std::println("Will install to {}", install_location.parent_location.string());
		std::println("Continue? (y/N)");
		std::string response;
		std::getline(std::cin, response);
		if (response.empty() || std::tolower(response[0]) == 'n')
			throw "Aborted installation";
		auto validated_install_maybe = validateInstallLocation(install_location);
		while (!validated_install_maybe.has_value()) {
			std::println("Invalid install location; {}", validated_install_maybe.error());
			std::println("Retry? (y/N)");
			std::string response;
			std::getline(std::cin, response);
			if (response.empty() || std::tolower(response[0]) == 'n')
				throw "Aborted installation";
			validated_install_maybe = validateInstallLocation(install_location);
		};
		ValidatedInstallLocation validated_install_location = validated_install_maybe.value();
		validated_install_location.doInstall();
		std::println("Finished installing to {}", validated_install_location.install_location.parent_location.string());
	}
	catch (const std::exception& exception) {
		std::println(std::cerr, "An exception was thrown during installation: {}", exception.what());
		return 1;
	}
	catch (const char* error_text) {
		std::println(std::cerr, "An error occured during installation: {}", error_text);
		return 1;
	}
	catch (const std::string& error_text) {
		std::println(std::cerr, "An error occured during installation: {}", error_text);
		return 1;
	}
	return 0;
}
