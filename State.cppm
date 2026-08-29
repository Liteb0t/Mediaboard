module;
#include "beast.hpp"
#include <boost/json.hpp>
#include <boost/smart_ptr.hpp>
#include <sodium.h>
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
	Board* createBoard(boost::json::object board_json) {
		std::lock_guard<std::mutex> lock(mutex);
		// boost::json::object board_json = request_json.at("board").as_object();
		auto board = std::make_unique<Board>(this, db, board_json);
		int new_board_id = board->getId();
		this->slug_to_board_id.emplace(board->getSlug(), new_board_id);
		this->boards.emplace(new_board_id, std::move(board));
		return this->boards.at(new_board_id).get();
	}

	std::string dumpAllGroups(const std::optional<FuzeHttp::Client>& client) const {
		std::lock_guard<std::mutex> lock(mutex);
		std::cout << "Dumping from ordered_groups_vec: ";

		boost::json::object groups_json;
		boost::json::array group_heirarchy_json;
		int group_editable_threshold;
		if (this->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS)))
			group_editable_threshold = this->getClientRank(client) + 1;
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
		std::lock_guard<std::mutex> lock(mutex);
		boost::json::object users_json;
		int client_rank = this->getClientRank(client);
		bool client_has_manage_permissions_permission = this->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS));
		for (auto& account : this->accounts) {
			int account_id = account.first;
			std::cout << account_id << ", ";
			int account_rank = this->getAccountRank(account_id);
			boost::json::object account_json {
				{"id", account_id},
				{"username", this->getUsernameFromAccount(account_id)},
				{"rank", account_rank}
			};
			boost::json::array user_groups_json;
			for (const int group_id : this->getOrderedGroupsContainingMember(account_id)) {
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
	std::unordered_map<int, std::unique_ptr<Board>> boards;
	std::unordered_map<std::string, int> slug_to_board_id; // when slug changes, old assosiation is not removed until reboot
	// std::vector<int> ordered_boards;
	// HTTP sessions. Client validates using a cookie
}; // class State
} // namespace Mediaboard
