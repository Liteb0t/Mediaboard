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
import FuzeHttp.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;

using namespace FuzeHttp;

// Forward declaration
// class WebsocketSession;
// namespace FuzeHttp{
// 	// class State;
// 	class Server;
// };

export namespace Mediaboard {
namespace Manager {

// Represents the shared server state
class State : public FuzeHttp::StateBase {
public:
	State(FuzeDBI::Connection* db) : FuzeHttp::StateBase(db) {
		// this->grantOwnerPrivileges(static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS));
	}
	void start() override {
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
	}
	// shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location_relative, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);

	const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	std::filesystem::path data_path;
	std::filesystem::path mediaboard_data_path;
	std::filesystem::path root_path;
	const std::filesystem::path& getMediaLocation() const { return media_location; }
}; // class State
}
} // namespace Mediaboard
