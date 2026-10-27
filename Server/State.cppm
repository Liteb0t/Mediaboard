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
export module Mediaboard.State;

import FuzeDBI;
import FuzeHttp.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;
import Mediaboard.Board;
import Mediaboard.File;
import Mediaboard.Permission;
#ifdef WITH_WEBRTC
import Mediaboard.Room;
#endif

using namespace FuzeHttp;

// Forward declaration
// class WebsocketSession;
// namespace FuzeHttp{
// 	// class State;
// 	class Server;
// };

export namespace Mediaboard {
#ifdef WITH_WEBRTC
struct TurnCredential {
	std::string username;
	std::string credential;
};
class IceServer {
public:
	inline static const std::unordered_set<std::string> TYPE_OPTIONS = {"STUN", "TURN"};
	inline static const std::unordered_set<std::string> TRANSPORT_OPTIONS = {"UDP", "TCP", "TLS"};
	static std::expected<IceServer, std::string> validateInput(boost::json::object json) {
		IceServer server;
		if (auto type_it = json.find("type"); type_it == json.end())
			return std::unexpected("Missing JSON field: type");
		else if (!type_it->value().is_string())
			return std::unexpected("JSON field 'type' must be an string");
		else {
			server.type = type_it->value().as_string();
			if (!TYPE_OPTIONS.contains(server.type))
				return std::unexpected(std::format("Type {} not recognised", server.type));
		}
		if (auto hostname_it = json.find("hostname"); hostname_it == json.end())
			return std::unexpected("Missing JSON field: hostname");
		else if (!hostname_it->value().is_string())
			return std::unexpected("JSON field 'hostname' must be an string");
		else {
			server.hostname = hostname_it->value().as_string();
			if (server.hostname.length() < 1)
				return std::unexpected("Hostname cannot be empty");
		}
		if (auto port_it = json.find("port"); port_it == json.end())
			return std::unexpected("Missing JSON field: port");
		else if (!port_it->value().is_int64())
			return std::unexpected("JSON field 'port' must be an int");
		else {
			server.port = port_it->value().as_int64();
			if (server.port < 0)
				return std::unexpected("Port cannot be negative");
			if (server.port == 0)
				server.port = 3478;
		}
		if (server.type == "TURN") {
			if (auto transport_it = json.find("transport"); transport_it == json.end())
				return std::unexpected("Missing JSON field: transport");
			else if (!transport_it->value().is_string())
				return std::unexpected("JSON field 'transport' must be an string");
			else {
				server.transport = transport_it->value().as_string();
				if (!TRANSPORT_OPTIONS.contains(server.transport))
					return std::unexpected(std::format("Transport option {} not recognised", server.transport));
			}
			if (auto shared_secret_it = json.find("shared_secret"); shared_secret_it == json.end())
				return std::unexpected("Missing JSON field: shared_secret");
			else if (!shared_secret_it->value().is_string())
				return std::unexpected("JSON field 'shared_secret' must be an string");
			else {
				server.shared_secret = shared_secret_it->value().as_string();
				if (server.shared_secret.length() < 1)
					return std::unexpected("shared_secret cannot be empty");
			}
		}
		return server;
	}
	// IceServer(std::string url, std::string shared_secret) : url(url), shared_secret(shared_secret) {}
	std::string type;
	std::string hostname;
	int port;
	std::string transport = "";
	std::string shared_secret = "";
	TurnCredential getCredentialForClient(const std::optional<Client>& client, std::chrono::seconds ttl = std::chrono::hours(24)) const {
		auto expiry = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() + ttl.count();
		TurnCredential turn_credential;
		if (client)
			turn_credential.username = std::format("{}:client_{}", expiry, client.value().id);
		else
			turn_credential.username = std::to_string(expiry);
		std::array<uint8_t, HMAC_SHA1_DIGEST_SIZE> digest;
		fuze_hmac_sha1(
			reinterpret_cast<const uint8_t*>(this->shared_secret.c_str()), this->shared_secret.length(),
			reinterpret_cast<const uint8_t*>(turn_credential.username.c_str()), turn_credential.username.length(),
			digest.data()
		);
		char encoded_base64[sodium_base64_ENCODED_LEN(digest.size(), sodium_base64_VARIANT_ORIGINAL)];
		sodium_bin2base64(
			encoded_base64, sizeof encoded_base64,
			digest.data(), digest.size(),
			sodium_base64_VARIANT_ORIGINAL
		);
		turn_credential.credential = encoded_base64;
		return turn_credential;
	}
	boost::json::object valuesAsJson() const {
		return {{
			{"type", type},
			{"hostname", hostname},
			{"port", port},
			{"transport", type == "TURN" ? transport : ""},
			{"shared_secret", type == "TURN" ? shared_secret : ""}
		}};
	}
	boost::json::object credentialsAsJson(const std::optional<Client>& client) const {
		TurnCredential turn_credential = getCredentialForClient(client);
		return {{
			{"urls", std::format("{}:{}:{}", type, hostname, port)},
			{"username", turn_credential.username},
			{"credential", turn_credential.credential}
		}};
	}
};
#endif
struct StateConfig {
	std::string thumbnail_file_extension;
	unsigned int thumbnail_size = 150;
	unsigned int file_size_limit_mb;
	bool convert_heic_to_jpg;
	bool avif_thumbnails;
	bool heic_thumbnails;
	bool svg_thumbnails;
	bool webp_thumbnails;
	bool mp4_thumbnails;
	bool webm_thumbnails;
	bool strip_metadata;
};
// Represents the shared server state
class State : public FuzeHttp::StateBase {
public:
	State(FuzeDBI::Connection* db) : FuzeHttp::StateBase(db) {
		this->grantOwnerPrivileges(static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS));
	}
	~State() {
		this->markExpiredTransientFilesAsDeleted();
	}
	// shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location_relative, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);
	StateConfig config;
	void start() override {
		this->setAdditionalImageFormatsFromConfig(this->config);
		this->cacheAllBoards();
		this->cacheAllFiles();
#ifdef WITH_WEBRTC
		this->addIceServers();
#endif
		// Mediaboard::Board main_board(this, db);
		// this->boards.emplace(0, main_board);
		// this->boards.at(0).cacheAllThreads();
	}
	void addMigrations() override;
	void setAdditionalImageFormatsFromConfig(const StateConfig& config) {
		if (config.avif_thumbnails)
			this->image_formats_to_create_thumbnails_for.emplace("image/avif");
		if (config.heic_thumbnails || config.convert_heic_to_jpg)
			this->image_formats_to_create_thumbnails_for.emplace("image/heic");
		if (config.svg_thumbnails)
			this->image_formats_to_create_thumbnails_for.emplace("image/svg+xml");
		if (config.webp_thumbnails)
			this->image_formats_to_create_thumbnails_for.emplace("image/webp");
		if (config.mp4_thumbnails)
			this->video_formats_to_create_thumbnails_for.emplace("video/mp4");
		if (config.webm_thumbnails)
			this->video_formats_to_create_thumbnails_for.emplace("video/webm");
	}
	bool canCreateThumbnailForImageFormat(const std::string_view mime_type) const {
		return this->image_formats_to_create_thumbnails_for.contains(std::string(mime_type));
	}
	bool canCreateThumbnailForVideoFormat(const std::string_view mime_type) const {
		return this->video_formats_to_create_thumbnails_for.contains(std::string(mime_type));
	}
#ifdef WITH_WEBRTC
	void addIceServers() {
		std::lock_guard<std::mutex> lock(mutex);
		std::println("[State] ICE servers from database...");
		for (auto ice_server_tuple : db->queryRows<std::tuple<std::string, std::string, int, std::string, std::string>>("SELECT type, hostname, port, transport, shared_secret FROM ice_servers")) {
			IceServer server;
			server.type = std::get<0>(ice_server_tuple);
			server.hostname = std::get<1>(ice_server_tuple);
			server.port = std::get<2>(ice_server_tuple);
			server.transport = std::get<3>(ice_server_tuple);
			server.shared_secret = std::get<4>(ice_server_tuple);
			std::println("{}:{}:{}", server.type, server.hostname, server.port);
			this->ice_servers.push_back(server);
		}
		std::println("Finished saving a total of {} ICE servers.", this->ice_servers.size());
	}
	boost::json::array getIceServersAsJson() const {
		std::lock_guard<std::mutex> lock(mutex);
		boost::json::array ice_servers_json = boost::json::array();
		for (auto& server : this->ice_servers) {
			ice_servers_json.emplace_back(server.valuesAsJson());
		}
		return ice_servers_json;
	}
	boost::json::array getIceCredentialsForClient(const std::optional<Client>& client) const {
		std::lock_guard<std::mutex> lock(mutex);
		boost::json::array ice_servers_json = boost::json::array();
		for (auto& server : this->ice_servers) {
			ice_servers_json.emplace_back(server.credentialsAsJson(client));
		}
		return ice_servers_json;
	}
	std::expected<void, std::string> markFilesAsDeleted(boost::json::object& json, const std::optional<Client>& client) {
		boost::json::array files_json = json.at("files").as_array();
		for (auto& filename : files_json) {
			std::string filename_str = std::string(filename.as_string().c_str());
			if (auto file_id_it = filename_to_file_id.find(filename_str); file_id_it != filename_to_file_id.end()) {
				File* file = files.at(file_id_it->second).get();
				if (!file->clientHasPermission(client, static_cast<int>(PERMISSION::NON_AUTHOR_DELETE_FILE)))
					return std::unexpected(std::format("Permission denied to delete file `{}`", filename_str));
				else
					file->markAsDeleted();
			}
			else
				return std::unexpected(std::format("File `{}` not found", filename_str));
		}
		return {}; // success
	}
	std::expected<void, std::string> restoreFiles(boost::json::object& json, const std::optional<Client>& client) {
		boost::json::array files_json = json.at("files").as_array();
		for (auto& filename : files_json) {
			std::string filename_str = std::string(filename.as_string().c_str());
			if (auto file_id_it = filename_to_file_id.find(filename_str); file_id_it != filename_to_file_id.end()) {
				File* file = files.at(file_id_it->second).get();
				if (!file->clientHasPermission(client, static_cast<int>(PERMISSION::NON_AUTHOR_DELETE_FILE)))
					return std::unexpected(std::format("Permission denied to restore file `{}`", filename_str));
				else
					file->restore();
			}
			else
				return std::unexpected(std::format("File `{}` not found", filename_str));
		}
		return {}; // success
	}
	std::expected<void, std::string> destroyFiles(boost::json::object& json, const std::optional<Client>& client) {
		boost::json::array files_json = json.at("files").as_array();
		for (auto& filename : files_json) {
			std::string filename_str = std::string(filename.as_string().c_str());
			if (auto file_id_it = filename_to_file_id.find(filename_str); file_id_it != filename_to_file_id.end()) {
				File* file = files.at(file_id_it->second).get();
				if (!file->clientHasPermission(client, static_cast<int>(PERMISSION::NON_AUTHOR_DELETE_FILE)))
					return std::unexpected(std::format("Permission denied to destroy file `{}`", filename_str));

				std::filesystem::path file_location = getMediaLocation() / file->name();
				file->destroy(getMediaLocation(), file_location);
			}
			else
				return std::unexpected(std::format("File `{}` not found", filename_str));
		}
		return {}; // success
	}
	void markExpiredTransientFilesAsDeleted() { // TODO run as async task with cooldown instead of on shutdown
		int initial_number_of_files = files.size();
		for (const auto& [file_id, file] : this->files | std::ranges::views::filter([this](const auto& file_id_to_file)->bool{
			return file_id_to_file.second->getStatus() == File::STATUS::TRANSIENT;
		})) {
			file->markAsDeleted();
		}
		std::println("[State::markExpiredTransientFilesAsDeleted] pruned {} files.", initial_number_of_files - files.size());
	}
	[[nodiscard]]std::expected<void, std::string> setIceServers(boost::json::array ice_servers_json) {
		std::lock_guard<std::mutex> lock(mutex);
		ice_servers.clear();
		db->query<void>("DELETE FROM ice_servers");
		for (auto ice_server_json : ice_servers_json) {
			auto server_maybe = IceServer::validateInput(ice_server_json.as_object());
			if (!server_maybe)
				return std::unexpected(server_maybe.error());
			IceServer server = server_maybe.value();
			ice_servers.push_back(server);
			db->query<void>("INSERT INTO ice_servers VALUES ($1, $2, $3, $4, $5)", server.type, server.hostname, server.port, server.transport, server.shared_secret);
		}
		return {};
	}
#endif
	void cacheAllBoards() { // no mutex needed because it's run once at startup
		std::print("[State] Retrieving boards from database...");
		for (auto board_tuple : db->queryRows<std::tuple<int, int, std::string, std::string>>("SELECT id, permission_object_id, slug, title FROM board WHERE deleted = FALSE")) {
			auto board = std::make_unique<Board>(this, db, std::get<0>(board_tuple), std::get<1>(board_tuple), std::get<2>(board_tuple), std::get<3>(board_tuple));
			std::print("{}, ", board->getId());
			board->cacheAllThreads();
			this->slug_to_board_id.emplace(std::get<2>(board_tuple), board->getId());
			this->boards.insert(std::make_pair(board->getId(), std::move(board)));
		}
		std::println("done. final list of boards:");
		for (const std::pair<std::string, int>& slug_board_id : slug_to_board_id)
			std::println("{} - {}", slug_board_id.first, slug_board_id.second);
	}
	static std::chrono::time_point<std::chrono::system_clock> getTimePointFromEpoch(int seconds_since_epoch) {
		std::chrono::seconds seconds(seconds_since_epoch);
		std::chrono::time_point<std::chrono::system_clock> time_point(seconds);
		return time_point;
	}
	void cacheAllFiles() {
		std::print("[State] Retrieving file references from database...");
		for (auto file_tuple : db->queryRows<std::tuple<int, int, std::string, int, std::optional<int>, std::optional<int>, std::optional<int>, std::optional<int>, std::optional<std::string>, std::optional<int>, int>>("SELECT id, permission_object_id, filename, created_at, coupled_board_id, coupled_post_id, width, height, thumbnail_file_extension, deleted_at, status FROM stored_file")) {
			int file_id = std::get<0>(file_tuple);
			std::string filename = std::get<2>(file_tuple);
			std::chrono::time_point<std::chrono::system_clock> created_at = getTimePointFromEpoch(std::get<3>(file_tuple));
			std::optional<int> coupled_board_id = std::get<4>(file_tuple);
			std::optional<int> coupled_post_id = std::get<5>(file_tuple);
			std::optional<std::chrono::time_point<std::chrono::system_clock>> deleted_at;
			if (std::get<9>(file_tuple))
				deleted_at = getTimePointFromEpoch(std::get<9>(file_tuple).value());
			File::STATUS status = static_cast<File::STATUS>(std::get<10>(file_tuple));

			PermissionObjectBase* file_permission_parent;
			if (coupled_board_id) {
				if (auto board_it = boards.find(coupled_board_id.value()); board_it != boards.end()) {
					file_permission_parent = board_it->second.get();
				}
				else {
					std::println(std::cerr, "Board not found from coupled_board_id {} on file {}. File therefore won't be loaded.", coupled_board_id.value(), file_id);
					continue;
				}
			}
			else
				file_permission_parent = this;

			auto file = std::make_unique<File>(file_permission_parent, db, file_id, std::get<1>(file_tuple), filename, created_at, coupled_board_id, coupled_post_id, std::get<6>(file_tuple), std::get<7>(file_tuple), std::get<8>(file_tuple), deleted_at, status);
			std::print("{}, ", file_id);
			File* inserted_file = this->insertFile(std::move(file));

			if (coupled_post_id) {
				int thread_id = db->query<int>("SELECT thread_id FROM message WHERE id = $1", coupled_post_id.value());
				std::print("Fetched thread {} from message {}", thread_id, coupled_post_id.value());
				int id_in_thread = db->query<int>("SELECT id_in_thread FROM message WHERE id = $1", coupled_post_id.value());
				std::print("Fetched id_in_thread {} from message {}", id_in_thread, coupled_post_id.value());
				int board_id = db->query<int>("SELECT board_id FROM thread WHERE id = $1", thread_id);
				if (board_id != coupled_board_id)
					throw std::format("Consistency check failed: coupled_board_id {} of stored_file {} is invalid", coupled_board_id.value(), file_id);
				std::print("Fetched board {} from thread {}", board_id, thread_id);
				if (auto board_it = boards.find(board_id); board_it == boards.end()) {
					std::print("Skipping (board {} not found)", board_id);
				}
				else {
					if (!board_it->second->threadExists(thread_id)) {
						std::print("Skipping (thread {} not found)", thread_id);
					}
					else {
						board_it->second->bindFileToPost(inserted_file, thread_id, id_in_thread);
					}
				}
			}
		}
		std::println("done.");
	}
	File* insertFile(std::unique_ptr<File>&& file) {
		std::lock_guard<std::mutex> lock(mutex);
		int file_id = file->getId();
		this->filename_to_file_id.emplace(file->name(), file_id);
		this->files.emplace(file_id, std::move(file));
		return this->files.at(file_id).get();
	}
	template<bool locked = true>
	std::optional<Board*> getBoardIfExists(int board_id) {
		if constexpr (locked) {
			std::lock_guard<std::mutex> lock(mutex);
		}
		if (auto it = boards.find(board_id); it != boards.end() && !(it->second.get()->isDeleted()))
			return it->second.get();
		else
			return {};
	}
	std::optional<Board*> getBoardIfExists(const std::string_view slug) {
		std::lock_guard<std::mutex> lock(mutex);
		if (auto it = slug_to_board_id.find(std::string(slug)); it == slug_to_board_id.end())
			return {};
		else {
			mutex.unlock();
			return getBoardIfExists(it->second);
		}
	}
	std::optional<Board*> getBoardIfExistsAndClientHasReadPermission(const std::string& slug, const std::optional<FuzeHttp::Client>& client) {
		std::optional<Board*> board = getBoardIfExists(slug);
		if (board && board.value()->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_BOARD)))
			return board;
		else
			return {};
	}
	std::optional<Board*> getBoardIfExistsAndClientHasReadPermission(int board_id, const std::optional<FuzeHttp::Client>& client) {
		std::optional<Board*> board = getBoardIfExists(board_id);
		if (board && board.value()->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_BOARD)))
			return board;
		else
			return {};
	}
	// int createThread(int board_id, boost::json::object thread_json, int author_client_id) {
	// 	return this->boards.at(board_id).createThread(thread_json, author_client_id);
	// }
	// int createMessage(int board_id, boost::json::object message_json, int author_client_id) {
	// 	return this->boards.at(board_id).createMessage(message_json, author_client_id);
	// }
	std::expected<Board*, std::string> createBoard(boost::json::object board_json, bool make_public) {
		std::lock_guard<std::mutex> lock(mutex);
		auto validated = Board::validateInput(board_json);
		if (!validated) {
			return std::unexpected(validated.error());
		}
		if (slug_to_board_id.contains(validated.value().slug))
			return std::unexpected("Board with slug already exists");
		// boost::json::object board_json = request_json.at("board").as_object();
		auto board = std::make_unique<Board>(this, db, validated.value());
		if (make_public && !board->clientHasPermission({}, static_cast<int>(PERMISSION::VIEW_BOARD)))
			board->setGroupPermission(static_cast<int>(BUILTIN_GROUPS::PUBLIC), static_cast<int>(PERMISSION::VIEW_BOARD), THREE_STATE_SETTING::ALLOW);
		else if (!make_public && board->clientHasPermission({}, static_cast<int>(PERMISSION::VIEW_BOARD)))
			board->setGroupPermission(static_cast<int>(BUILTIN_GROUPS::PUBLIC), static_cast<int>(PERMISSION::VIEW_BOARD), THREE_STATE_SETTING::DENY);
		int new_board_id = board->getId();
		this->slug_to_board_id.emplace(board->getSlug(), new_board_id);
		this->boards.emplace(new_board_id, std::move(board));
		return this->boards.at(new_board_id).get();
	}

	std::string dumpAllGroups(const std::optional<FuzeHttp::Client>& client) const {
		std::lock_guard<std::mutex> lock(permission_mutex);
		std::cout << "Dumping from ordered_groups_vec: ";

		boost::json::object groups_json;
		boost::json::array group_heirarchy_json;
		int group_editable_threshold;
		if (this->clientHasPermissionUnlocked(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS)))
			group_editable_threshold = this->getClientRankUnlocked(client) + 1;
		else
			group_editable_threshold = this->getOrderedGroups()->size();
		for (int i = 0; i < this->getOrderedGroups()->size(); i++) {
			int group_id = (*(this->getOrderedGroups()))[i];
			group_heirarchy_json.emplace_back(group_id);
			std::cout << group_id << ", ";
			boost::json::object group_json{
				{"id", group_id},
				{"name", this->getGroupUnlocked(group_id)->getName()},
				{"heirarchy_editable", i >= group_editable_threshold && (group_id != static_cast<int>(BUILTIN_GROUPS::USERS) && group_id != static_cast<int>(BUILTIN_GROUPS::PUBLIC))},
				{"permission_editable", i >= group_editable_threshold}
			};
			groups_json.emplace(std::to_string(group_id), group_json);
		}
		std::cout << " done." << std::endl;


		return boost::json::serialize(boost::json::object{
			{"groups", groups_json},
			{"group_heirarchy", group_heirarchy_json}
		});
	}
	std::string dumpAllUsers(const std::optional<FuzeHttp::Client>& client) const {
		std::lock_guard<std::mutex> lock(permission_mutex);
		boost::json::object users_json;
		int client_rank = this->getClientRankUnlocked(client);
		bool client_has_manage_permissions_permission = this->clientHasPermissionUnlocked(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS));
		for (auto& account : this->accounts) {
			int account_id = account.first;
			std::cout << account_id << ", ";
			int account_rank = this->getAccountRankUnlocked(account_id);
			boost::json::object account_json {
				{"id", account_id},
				{"username", account.second.username},
				{"rank", account_rank}
			};
			boost::json::array user_groups_json;
			for (const int group_id : this->getOrderedGroupsContainingMemberUnlocked(account_id)) {
				const Group* group = this->getGroupUnlocked(group_id);
				user_groups_json.emplace_back(boost::json::object{
					{"id", group->getId()},
					{"name", group->getName()}
				});
			}
			account_json.emplace("groups", user_groups_json);
			account_json.emplace("permission_editable", client_has_manage_permissions_permission && client_rank < account_rank);
			users_json.emplace(std::to_string(account_id), account_json);
		}
		std::cout << " done." << std::endl;

		return boost::json::serialize(boost::json::object{
			{"users", users_json}
		});
	}
	boost::json::object getBoardsAsJson(const std::optional<FuzeHttp::Client>& client) const {
		std::lock_guard<std::mutex> lock(mutex);
		boost::json::array boards_json = boost::json::array();
		for (auto& [board_id, board] : this->boards) {
			if (!board->isDeleted()) {
				if (board->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_BOARD)) && !board->isDeleted()) {
					boards_json.emplace_back(board->asJson(client));
				}
			}
		}
		std::println("[State] Finished assembling boards list into JSON");
		return {
			{"type", "board_list"},
			{"boards", boards_json},
			{"client_permissions", {
				{"create_board", this->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_BOARD))},
			}}
		};
	}
	// TODO check client permission to view files
	boost::json::object getFilesAsJson(std::optional<Board*> board, bool get_recently_deleted) const {
		std::lock_guard<std::mutex> lock(mutex);
		boost::json::array files_json = boost::json::array();
		for (auto& [file_id, file] : this->files) {
			if (board && !file->isCoupledToBoard(board.value()->getId()))
				continue;
			if (file->getStatus() == File::STATUS::DESTROYED)
				continue;
			if ((file->getStatus() == File::STATUS::CONDEMNED) == get_recently_deleted)
				files_json.emplace_back(file->asJson());
		}
		std::println("[State] Finished assembling files list into JSON");
		return {
			{"files", files_json}
		};
	}
	std::optional<const Group*> getHighestRankGroupForClient(const std::optional<FuzeHttp::Client>& client) const {
		std::lock_guard<std::mutex> lock(mutex);
		std::optional<const Group*> group_maybe;
		if (client && client.value().account_id) {
			auto groups = this->getOrderedGroupsContainingMemberUnlocked(client.value().account_id.value());
			std::println("[getHighestRankGroupForClient] number of groups: {}", groups.size());
			if (groups.size() >= 1)
				group_maybe = this->getGroupUnlocked(groups[0]);
		}
		return group_maybe;
	}
	boost::json::object getClientAsJson(const std::optional<Client> client, bool has_cookie) const {
		boost::json::object client_json = {{
			{"server_permissions", {
				{"manage_permissions", this->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS))},
				// will be handled by Board-level perms
				// {"create_thread", state->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_THREAD))},
				{"upload_file", this->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE))}
			}},
			{"has_cookie", has_cookie}
		}};
		std::optional<const Group*> group_maybe = getHighestRankGroupForClient(client);
		if (group_maybe)
			client_json.emplace("highest_ranked_group", group_maybe.value()->asJson());
		return client_json;
	}
	std::expected<Message*, std::string> createMessage(const boost::json::object message_json, Board* board, Thread* thread, const FuzeHttp::Client client) {
		if (auto message_maybe = validateMessageInThread(message_json, board, thread, client); !message_maybe)
			return std::unexpected(message_maybe.error());
		else {
			Message* message = board->createMessage(message_maybe.value(), client);
			return message;
		}
	}
	std::expected<Thread*, std::string> createThread(const boost::json::object thread_json, Board* board, const FuzeHttp::Client client) {
		if (auto thread_maybe = validateThread(thread_json, board, client); !thread_maybe)
			return std::unexpected(thread_maybe.error());
		else
			return board->createThread(thread_maybe.value(), client.id);
	}
	std::expected<File*, std::string> validateFile(const boost::json::object json) {
		File::Validated validated_file;
		if (auto filename_it = json.find("filename"); filename_it == json.end())
			return std::unexpected("Missing JSON field: filename");
		else if (!filename_it->value().is_string())
			return std::unexpected("JSON field 'filename' must be an string");
		else {
			validated_file.filename = filename_it->value().as_string();
			if (validated_file.filename.size() < 1 || validated_file.filename.size() > static_cast<size_t>(File::MAX_FILE_NAME_WITH_UUID))
				return std::unexpected(std::format("Filename length {} is not between 1 and {}", validated_file.filename.length(), File::MAX_FILE_NAME_WITH_UUID));
			if (auto it = filename_to_file_id.find(validated_file.filename); it == filename_to_file_id.end()) {
				return std::unexpected(std::format("File {} not found", validated_file.filename));
			}
			else
				validated_file.id = it->second;
		}

		if (json.contains("width") || json.contains("height") || json.contains("thumbnail_file_extension")) {
			if (auto width_it = json.find("width"); width_it == json.end())
				return std::unexpected("Missing JSON field: width");
			else if (!width_it->value().is_int64())
				return std::unexpected("JSON field 'width' must be an int");
			else
				validated_file.width = width_it->value().as_int64();
			if (auto height_it = json.find("height"); height_it == json.end())
				return std::unexpected("Missing JSON field: height");
			else if (!height_it->value().is_int64())
				return std::unexpected("JSON field 'height' must be an int");
			else
				validated_file.height = height_it->value().as_int64();

			if (auto thumbnail_file_extension_it = json.find("thumbnail_file_extension"); thumbnail_file_extension_it == json.end())
				return std::unexpected("Missing JSON field: thumbnail_file_extension");
			else if (!thumbnail_file_extension_it->value().is_string())
				return std::unexpected("JSON field 'thumbnail_file_extension' must be an string");
			else {
				validated_file.thumbnail_file_extension = thumbnail_file_extension_it->value().as_string();
				// TODO check if file extension is in supported formats
				// TODO sanitise filename. Frontend handles this but not if the API is used directly
				if (validated_file.thumbnail_file_extension->length() < 1 || validated_file.thumbnail_file_extension->length() > File::MAX_FILE_NAME)
					return std::unexpected(std::format("thumbnail_file_extension length {} is not between 1 and {}", validated_file.thumbnail_file_extension->length(), File::MAX_FILE_NAME));
			}
		}
		if (auto file_it = files.find(validated_file.id); file_it == files.end())
			return std::unexpected(std::format("File with ID {} not found from filename {}", validated_file.id, validated_file.filename));
		else
			return file_it->second.get();
		// return validated_file;
	}
	void sendToWebRTC(std::string message);
	void clearWebsockets();
	void setBoardSlug(int board_id, const std::string& new_slug) {
		std::lock_guard<std::mutex> lock(mutex);
		this->boards.at(board_id)->setSlug(new_slug);
		this->slug_to_board_id.emplace(new_slug, board_id);
	}

	const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	const std::filesystem::path& getMediaLocation() const { return media_location; }
#ifdef WITH_WEBRTC
	const std::vector<IceServer> getIceServers() const { return ice_servers; }

	// object with shared_ptr owns the room as its stored in a weak_ptr here
	std::shared_ptr<Room> createRoom(std::optional<Board*> board) {
		int new_room_id = room_id_seq++;
		PermissionObjectBase* room_permission_parent;
		if (board) room_permission_parent = board.value(); else room_permission_parent = this;
		auto room = std::make_shared<Room>(this,room_permission_parent, new_room_id/*, this->getExecutor()*/);
		if (board)
			room->coupleToBoard(board.value()->getId());
		rooms.emplace(new_room_id, room);
		return room;
	}
	std::optional<std::shared_ptr<Room>> getSharedRoomIfExists(int room_id) const {
		auto it = rooms.find(room_id);
		if (it != rooms.end()) {
			if (auto strong_room = it->second.lock())
				return strong_room;
			else
				std::println(std::cerr, "Couldn't lock room {}", room_id);
		}
		return {};
	}
	// TODO check client permission to view rooms
	boost::json::object getRoomsAsJson(std::optional<Board*> board) const {
		boost::json::array rooms_json = boost::json::array();
		for (auto& [room_id, weak_room] : this->rooms) {
			if (auto room = weak_room.lock()) {
				if (board && !room->isCoupledToBoard(board.value()->getId()))
					continue;
				rooms_json.push_back(room->asJson());
			}
		}
		return {{"rooms", rooms_json}};
	}
	void clearEmptyRooms() { // TODO use this at some point
		std::erase_if(this->rooms, [this](const auto& id_weak_room_pair)->bool{
			return id_weak_room_pair.second.expired();
		});
	}

	int room_id_seq = 0;
	std::unordered_map<int, std::weak_ptr<Room>> rooms;
#endif
private:
	std::expected<Thread::Validated, std::string> validateThread(const boost::json::object json, const Board* board, const FuzeHttp::Client client) {
		Thread::Validated validated;

		if (auto post_zero_it = json.find("post_zero"); post_zero_it == json.end())
			return std::unexpected("Missing JSON field: post_zero");
		else if (!post_zero_it->value().is_object())
			return std::unexpected("JSON field 'post_zero' must be an object");
		else {
			boost::json::object post_zero = post_zero_it->value().as_object();
			if (auto message = validateMessage(post_zero, board, client))
				validated.post_zero_validated = message.value();
			else
				return std::unexpected(message.error());
		}
		if (validated.post_zero_validated.files.size() > 0) {
			// TODO move permission checking to another place, and have customisable upload limits
			if (!board->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE)))
				return std::unexpected(std::format("Client {} does not have permission to upload files", client.id));
		}
		return validated;
	}
	std::expected<Message::Validated, std::string> validateMessage(const boost::json::object json, const Board* board, const FuzeHttp::Client client, Message::Validated validated = {}) {
		if (auto highest_ranked_group_id_it = json.find("highest_ranked_group_id"); highest_ranked_group_id_it != json.end()) {
			if (!highest_ranked_group_id_it->value().is_int64())
				return std::unexpected("JSON field 'highest_ranked_group_id' must be an int");
			if (!client.account_id)
				return std::unexpected("Including rank in message requires a logged-in user."); // TODO maybe silently skip instead
			int account_id = client.account_id.value();
			int highest_ranked_group_id = highest_ranked_group_id_it->value().as_int64();
			std::lock_guard<std::mutex> lock(permission_mutex);
			if (!this->groupExistsUnlocked(highest_ranked_group_id))
				return std::unexpected(std::format("Group with ID {} does not exist", highest_ranked_group_id));
			const Group* group = this->getGroupUnlocked(highest_ranked_group_id);
			if (!group->containsMember(account_id))
				return std::unexpected(std::format("Cannot attach rank because account {} is not in group {}", account_id, highest_ranked_group_id));
			validated.highest_ranked_group_name = group->getName();
		}

		if (auto name_it = json.find("name"); name_it == json.end())
			return std::unexpected("Missing JSON field: name");
		else if (!name_it->value().is_string())
			return std::unexpected("JSON field 'name' must be an string");
		else {
			validated.name = name_it->value().as_string();
			if (validated.name.length() > Message::MAX_NAME)
				return std::unexpected(std::format("Name length {} must be less than {}", validated.name.length(), Message::MAX_NAME));
		}

		size_t number_of_files;
		if (auto files_it = json.find("files"); files_it == json.end())
			return std::unexpected("Missing JSON field: files");
		else if (!files_it->value().is_array())
			return std::unexpected("JSON field 'files' must be an array");
		else {
			boost::json::array files_json = files_it->value().as_array();
			number_of_files = files_json.size();
			if (number_of_files > Message::MAX_NUMBER_OF_FILES)
				return std::unexpected(std::format("Cannot attach more than {} files", Message::MAX_NUMBER_OF_FILES));
			for (boost::json::value file_val : files_json) {
				if (!file_val.is_object())
					return std::unexpected("file_val must be an object");
				boost::json::object& file_obj = file_val.as_object();
				auto file_maybe = validateFile(file_obj);
				if (!file_maybe)
					return std::unexpected(file_maybe.error());
				validated.files.push_back(file_maybe.value());
			}
		}

		if (auto content_it = json.find("content"); content_it == json.end())
			return std::unexpected("Missing JSON field: content");
		else if (!content_it->value().is_string())
			return std::unexpected("JSON field 'content' must be an string");
		else {
			validated.content = content_it->value().as_string();
			if ((validated.content.length() < 1/* && number_of_files == 0*/) || validated.content.length() > Message::MAX_CONTENT)
				return std::unexpected(std::format("Content length {} is not between 1 and {}", validated.content.length(), Message::MAX_CONTENT));
		}

		return validated;
	}
	std::expected<Message::Validated, std::string> validateMessageInThread(const boost::json::object json, const Board* board, const Thread* thread, const FuzeHttp::Client client) {
		Message::Validated validated;
		if (auto thread_id_it = json.find("thread_id"); thread_id_it == json.end())
			return std::unexpected("Missing JSON field: thread_id");
		else if (!thread_id_it->value().is_int64())
			return std::unexpected("JSON field 'thread_id' must be int64");
		else {
			validated.thread_id = thread_id_it->value().as_int64();
			// if (!board->threadExists(validated.thread_id))
			// 	return std::unexpected(std::format("Thread {} not found in board {}", validated.thread_id, board->getSlug()));
			if (validated.thread_id != thread->getId())
				return std::unexpected(std::format("Thread ID in JSON {} does not match thread ID in URL {}", validated.thread_id, thread->getId()));
		}
		if (validated.files.size() > 0) {
			// TODO move permission checking to another place, and have customisable upload limits
			if (!thread->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE)))
				return std::unexpected(std::format("Client {} does not have permission to upload files", client.id));
		}
		return validateMessage(json, board, client, validated);
	}
	std::unordered_set<std::string> image_formats_to_create_thumbnails_for = {"image/bmp", "image/gif", "image/vnd.microsoft.icon", "image/jpeg", "image/jxl", "image/png"};
	std::unordered_set<std::string> video_formats_to_create_thumbnails_for;

	// we use mutex from FuzeHttp::StateBase instead
	// std::mutex mutex_;

	// std::unordered_map<int, Board> boards;
#ifdef WITH_WEBRTC
	std::vector<IceServer> ice_servers;
#endif
	std::unordered_map<int, std::unique_ptr<Board>> boards;
	std::unordered_map<std::string, int> slug_to_board_id; // when slug changes, old assosiation is not removed until reboot
	std::unordered_map<int, std::unique_ptr<File>> files;
	std::unordered_map<std::string, int> filename_to_file_id;
	// std::vector<int> ordered_boards;
	// HTTP sessions. Client validates using a cookie
}; // class State
} // namespace Mediaboard
