// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <filesystem>
#include <iostream>
#include <memory>
#include <print>
#include <string>
export module Mediaboard.Manager.Config;
import Mediaboard.Manager.State;
import FuzeHttp.PermissionObject;
import FuzeHttp.ProgramOptions;
import FuzeHttp.Server;
import HomeDirLibrary;

using namespace FuzeHttp;

export namespace Mediaboard::Manager {
void addProgramOptions(ProgramOptions* options) {
	options->add<State>("test", [](State* state){
		std::println("TEST FUNCTION CALLED");
		std::println("server program location: {}", state->program_location.string());
	}, "Testing extra callbacks");
	options->add<State>("install", [](State* state){
		std::println("server program location: {}", state->program_location.string());
		std::filesystem::path program_path = state->program_location.parent_path();
		// set different default for different operating systems
		std::filesystem::path data_directory = HomeDirLibrary::getDataDir();
		std::filesystem::path default_install_prefix = data_directory;

		std::filesystem::path install_parent_location = default_install_prefix / "Fuze Mediaboard";
		std::println("Install parent location will be \"{}\"", install_parent_location.string());
		std::println("Press enter to confirm, or type in a different location:");
		std::string user_input;
		std::getline(std::cin, user_input);
		if (!user_input.empty())
			install_parent_location = user_input;
		std::filesystem::path install_version_location = install_parent_location / "versions" / state->server_version;
		if (std::filesystem::exists(install_version_location))
			throw std::format("Version is already installed at {}", install_version_location.string());
		std::filesystem::create_directories(install_version_location);
		std::println("Will copy {} to {}", program_path.string(), install_version_location.string());
		std::filesystem::copy(program_path, install_version_location, std::filesystem::copy_options::recursive);

		std::filesystem::create_directories(install_parent_location / "servers");

		std::println("Installation complete.");
	}, "Testing extra callbacks");
	;
}
} // export namespace Mediaboard
