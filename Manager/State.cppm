// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#ifdef WITH_WEBRTC
#include "hmac.h"
#endif
#include "beast.hpp"
#include <boost/json.hpp>
#include <boost/smart_ptr.hpp>
#include <sodium.h>
#include <expected>
#include <filesystem>
#include <iostream>
#include <list>
#include <mutex>
#include <print>
#include <ranges>
#include <string>
#include <unordered_set>
export module Mediaboard.Manager.State;

import FuzeDBI;
import FuzeHttp.Core;
import FuzeHttp.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;
import Mediaboard.Manager.Operations;
import Mediaboard.Manager.MediaboardServerInformation;

using namespace FuzeHttp;

export namespace Mediaboard::Manager {

// Represents the shared server state
class State : public FuzeHttp::StateBase {
public:
	State(FuzeDBI::Connection* db) : FuzeHttp::StateBase(db) {
		// this->grantOwnerPrivileges(static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS));
	}

	std::expected<void, std::string> updateServers() {
		std::println("[updateServers] Beginning...");
		std::unordered_set<std::string> site_names;
		std::lock_guard<std::mutex> lock(mutex);
		if (!std::filesystem::exists(servers_folder)) {
			return std::unexpected(std::format("servers_folder \"{}\" not found", servers_folder.string()));
		}
		try {
			for (const auto& dir_entry : std::filesystem::directory_iterator{servers_folder}) {
				if (auto server_info_maybe = parseServerDirectory(dir_entry.path())) {
					MediaboardServerInformation* server = server_info_maybe.value().get();
					std::string site_name = server->site_name;
					site_names.emplace(site_name);
					if (this->mediaboard_servers.contains(site_name))
						continue;
					if (server->server_port != -1 && serverWithPortExists(server->server_port)) {
						server->is_valid = false;
						server->additional_information.emplace_back(std::format("Server with port {} already exists", server->server_port));
					}
					this->mediaboard_servers.emplace(site_name, std::move(server_info_maybe.value()));
				}
				else {
					std::println(std::cerr, "Error when reading server directory \"{}\"", dir_entry.path().string());
				}
			}
			if (site_names.size() != this->mediaboard_servers.size()) {
				std::println("Removing stale servers from memory...");
				std::erase_if(this->mediaboard_servers, [this, &site_names](const auto& server_pair){
					return !site_names.contains(server_pair.first);
				});
			}
		}
		catch (const std::exception& exception) {
			std::println(std::cerr, "[updateServers] An exception was thrown: {}", exception.what());
			return std::unexpected(std::format("[updateServers] An exception was thrown: {}", exception.what()));
		}
		std::println("[updateServers] Scan complete.");
		return {};
	}
	boost::json::array getServersAsJson() {
		std::lock_guard<std::mutex> lock(mutex);
		boost::json::array mediaboard_servers_json = boost::json::array();
		for (const auto& [site_name, server] : this->mediaboard_servers) {
			mediaboard_servers_json.emplace_back(server->asJson());
		}
		return mediaboard_servers_json;
	}
	void start() override {
		{
			std::lock_guard<std::mutex> lock(mutex);
			data_path = this->document_root.parent_path();
			std::println("Data path: {}", data_path.string());
			mediaboard_data_path = data_path.parent_path() / "MediaboardServer";
			std::println("Mediaboard data path: {}", mediaboard_data_path.string());
			std::filesystem::path program_version_folder = this->program_location.parent_path();
			std::println("program_version_folder: {}", program_version_folder.string());
			if (this->server_version != program_version_folder.filename().string())
				throw std::format("server_version {} does not match program_version_folder.filename() {}. MediaboardManager must be installed with --install first", this->server_version, program_version_folder.filename().string());
			root_path = this->program_location.parent_path().parent_path().parent_path();
			std::println("Root path: {}", root_path.string());
			this->servers_folder = root_path / "servers";
		}
		if (auto res = updateServers(); !res)
			throw std::runtime_error(res.error());
	}
	std::expected<void, Response> createServer(std::string new_server_name) {
		std::lock_guard<std::mutex> lock(mutex);
		// TODO do case insentitive check (macOS)
		if (this->mediaboard_servers.contains(new_server_name))
			return std::unexpected(Response{.status=http::status::bad_request, .error_message=std::format("Server with name \"{}\" already exists", new_server_name)});
		if (!std::filesystem::exists(servers_folder)) {
			return std::unexpected(Response{.status=http::status::bad_request, .error_message=std::format("servers_folder \"{}\" not found", servers_folder.string())});
		}
		std::filesystem::path new_server_folder = servers_folder / new_server_name;
		if (std::filesystem::exists(new_server_folder)) {
			return std::unexpected(Response{.status=http::status::bad_request, .error_message=std::format("new_server_folder \"{}\" already exists", new_server_folder.string())});
		}
		std::filesystem::create_directory(new_server_folder);
		// TODO modify config.ini to set a different site_name and port
		std::filesystem::copy(this->mediaboard_data_path / "config.ini", new_server_folder);
		std::filesystem::path new_config_file_path = new_server_folder / "config.ini";
		std::ofstream config_writer(new_config_file_path.string(), std::ios_base::app);
		if (!config_writer)
			return std::unexpected(Response{.status=http::status::internal_server_error, .error_message = std::format("Couldn't write to \"{}\"", new_config_file_path.string())});
		config_writer << "site_name = " << new_server_name << std::endl;
		config_writer << "server_port = " << getFreePort() << std::endl;
		return {};
	}
	// shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location_relative, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);

	// const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	// const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	std::filesystem::path data_path;
	std::filesystem::path mediaboard_data_path;
	std::filesystem::path root_path;
	std::filesystem::path servers_folder;
	// const std::filesystem::path& getMediaLocation() const { return media_location; }
private:
	int getFreePort() const {
		int port = 8300;
		while (serverWithPortExists(port))
			++port;
		return port;
	}
	bool serverWithPortExists(const int port) const {
		std::unordered_set<int> used_ports;
		for (const auto& [name, server] : mediaboard_servers) {
			used_ports.emplace(server->server_port);
		}
		return used_ports.contains(port);
	}
	std::unordered_map<std::string, std::unique_ptr<MediaboardServerInformation>> mediaboard_servers;
}; // class State
} // export namespace Mediaboard::Manager
