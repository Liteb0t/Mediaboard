// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
export module Mediaboard.Manager.MediaboardServerInformation;

export namespace Mediaboard::Manager {

// Will scan 'servers' directory on startup, and update the database accordingly
struct MediaboardServerInformation {
	MediaboardServerInformation(const std::filesystem::path& data_directory) : data_directory(data_directory) {
		if (!std::filesystem::exists(getLockFilePath()))
			std::ofstream lock_file(getLockFilePath().string());
		this->file_lock = boost::interprocess::file_lock(getLockFilePath().string().c_str());
		if (!file_lock.try_lock()) {
			is_valid = false;
			additional_information.push_back("Lock is used by another process.");
		}
	}
	std::string site_name;
	const std::filesystem::path data_directory; // containing config.ini
	int server_port;
	bool is_valid = true;
	bool is_running = false;
	bool isRunning() const { return is_running; }
	// std::expected<void, std::string> lockTheDoor() { // expected to be called ONCE
	// 	// else if (server->lock_is_held)
	// 	// 	return std::unexpected(std::format("Server with name \"{}\" has a lock which is held by this daemon.", site_name));
	// 	if (!file_lock.try_lock())
	// 		return std::unexpected(std::format("Server with name \"{}\" is probably already running (The lock file is held).", site_name));
	// 	else
	// 		server->lock_is_held = true; // TODO find a way to turn it off when server shuts down
	// }
	boost::json::array additional_information = {};
	boost::json::object asJson() const {
		return {{
			{"site_name", site_name},
			{"data_directory", data_directory.string()},
			{"server_port", server_port},
			{"is_valid", is_valid},
			{"is_running", is_running},
			{"additional_information", additional_information}
		}};
	}
	std::filesystem::path getConfigFilePath() const {
		return this->data_directory / "config.ini";
	}
	std::filesystem::path getLogFilePath() const {
		return this->data_directory / "log.txt";
	}
	std::filesystem::path getLockFilePath() const {
		return this->data_directory / "lock";
	}
	boost::interprocess::file_lock file_lock;
};
}
