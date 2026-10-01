module;
#include <boost/program_options.hpp>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
export module Mediaboard.Manager.Operations;
import Mediaboard.Manager.MediaboardServerInformation;

export namespace Mediaboard::Manager {

std::expected<std::unique_ptr<MediaboardServerInformation>, std::string> parseServerDirectory(const std::filesystem::path& server_directory) {
	std::println("Scanning server directory \"{}\"", server_directory.string());
	std::filesystem::path server_config_file = server_directory / "config.ini";
	std::ifstream server_config_file_ifstream(server_config_file.string());
	if (!server_config_file_ifstream)
		return std::unexpected(std::format("Config file not found at \"{}\"", server_config_file.string()));
	// std::string site_name;
	// unsigned int server_port;
	boost::program_options::options_description options;
	options.add_options()
		// from FuzeHttp
		("site_name", boost::program_options::value<std::string>())
		("server_port", boost::program_options::value<int>())
		;
	boost::program_options::variables_map variable_map;
	try {
		auto parsed = boost::program_options::parse_config_file(server_config_file_ifstream, options, true /*this skips unrecognised options*/);
		boost::program_options::store(parsed, variable_map);
		boost::program_options::notify(variable_map);
	}
	catch (const boost::program_options::error& error) {
		return std::unexpected("boost::program_options error");
	}
	auto server_info = std::make_unique<MediaboardServerInformation>();
	server_info->data_directory = server_directory;
	if (variable_map.count("site_name"))
		server_info->site_name = variable_map["site_name"].as<std::string>();
	else {
		server_info->site_name = std::format("(unset) in folder {}", server_directory.filename().string());
		std::println(std::cerr, "Required option site_name not set in config.ini");
		server_info->is_valid = false;
		server_info->additional_information.push_back("Required option site_name not set in config.ini");
	}
	if (variable_map.count("server_port"))
		server_info->server_port = variable_map["server_port"].as<int>();
	else {
		server_info->server_port = -1;
		std::println(std::cerr, "Required option server_port not set in config.ini");
		server_info->is_valid = false;
		server_info->additional_information.push_back("Required option server_port not set in config.ini");
	}
	return server_info;
}

} // export namespace Mediaboard::Manager
