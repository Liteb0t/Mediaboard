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
import Mediaboard.Permission;

using namespace FuzeHttp;
using namespace FuzeHttp::Migrations;

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
	// shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location_relative, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);
	StateConfig config;
	void start() override {
		this->setAdditionalImageFormatsFromConfig(this->config);
		this->cacheAllBoards();
		this->addIceServers();
		// Mediaboard::Board main_board(this, db);
		// this->boards.emplace(0, main_board);
		// this->boards.at(0).cacheAllThreads();
	}
	std::list<std::unique_ptr<Migration>> addMigrations() override;
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
		for (auto thread_tuple : db->queryRows<std::tuple<int, int, std::string, std::string>>("SELECT id, permission_object_id, slug, title FROM board WHERE deleted = FALSE")) {
			// Board board(this, db, std::get<0>(thread_tuple), std::get<1>(thread_tuple), std::get<2>(thread_tuple), std::get<3>(thread_tuple));
			auto board = std::make_unique<Board>(this, db, std::get<0>(thread_tuple), std::get<1>(thread_tuple), std::get<2>(thread_tuple), std::get<3>(thread_tuple));
			std::print("{}, ", board->getId());
			board->cacheAllThreads();
			this->slug_to_board_id.emplace(std::get<2>(thread_tuple), board->getId());
			this->boards.insert(std::make_pair(board->getId(), std::move(board)));
		}
		std::println("done. final list of boards:");
		for (const std::pair<std::string, int>& slug_board_id : slug_to_board_id)
			std::println("{} - {}", slug_board_id.first, slug_board_id.second);
	}

	std::optional<Board*> getBoardIfExists(int board_id) {
		std::lock_guard<std::mutex> lock(mutex);
		if (auto it = boards.find(board_id); it != boards.end() && !(it->second.get()->isDeleted()))
			return it->second.get();
		else
			return {};
	}
	std::optional<Board*> getBoardIfExists(const std::string& slug) {
		std::lock_guard<std::mutex> lock(mutex);
		if (auto it = slug_to_board_id.find(slug); it == slug_to_board_id.end())
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
				{"name", this->getGroup(group_id)->getName()},
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
				const Group* group = this->getGroup(group_id);
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
			if (board->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_BOARD)) && !board->isDeleted()) {
				boards_json.emplace_back(board->asJson(client));
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
	// void websocketRead (FuzeHttp::WebsocketSession* session) override;
	void sendToThread (std::string message, Board* board, int thread_id) { // TODO move to Board
		// Put the message in a shared pointer so we can re-use it for each client
		auto const ss = std::make_shared<std::string const>(std::move(message));

		// Make a local list of all the weak pointers representing
		// the sessions, so we can do the actual sending without
		// holding the mutex:
		std::vector<std::weak_ptr<FuzeHttp::WebsocketSession>> v;
		{
			std::lock_guard<std::mutex> lock(mutex);
			v.reserve(websocket_sessions.size());
			board->removeUnauthorizedListenersFromThread(thread_id);
			for(auto p : board->getListenersFromThread(thread_id))
				v.emplace_back(p->weak_from_this());
		}

		// For each session in our local list, try to acquire a strong
		// pointer. If successful, then send the message on that session.
		for(auto const&wp : v) {
			if(auto sp = wp.lock())
				sp->send(ss);
		}
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
	const std::vector<IceServer> getIceServers() const { return ice_servers; }
	// const std::filesystem::path& getProgramLocation() const { return program_location; }
	// const char* getSecret() const { return this->secret_base64; }
private:
	// const std::filesystem::path media_location;
	// const std::filesystem::path program_location;
	// char secret_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	// FuzeDBI::Connection* fuze_dbi;
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
	// std::vector<int> ordered_boards;
	// HTTP sessions. Client validates using a cookie
}; // class State
} // namespace Mediaboard
