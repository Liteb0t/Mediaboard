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
#include <string>
#include <unordered_set>
export module Mediaboard.State;

import FuzeDBI;
import FuzeHttp.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;
import Mediaboard.Board;

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
class State : public FuzeHttp::State {
public:
	State(FuzeDBI::Connection* db) : FuzeHttp::State(db) {}
	// shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location_relative, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);
	StateConfig config;
	void start() override {
		this->setAdditionalImageFormatsFromConfig(this->config);
		Mediaboard::Board main_board(this, db);
		this->boards.emplace(0, main_board);
		this->boards.at(0).cacheAllThreads();
	}
	std::list<std::unique_ptr<Migration>> addMigrations() override {
		std::list<std::unique_ptr<Migration>> migrations;
		migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.1", "ALTER TABLE message_file ADD COLUMN width INTEGER;"
		"ALTER TABLE message_file ADD COLUMN height INTEGER;")));
		migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.2", "ALTER TABLE message_file ADD COLUMN thumbnail_file_extension TEXT;"
		"ALTER TABLE thread ADD COLUMN message_id_seq INTEGER DEFAULT 0;"
		"UPDATE thread SET message_id_seq = 1000")));
		// migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2.2", "CREATE TABLE sql_test(ting TEXT)")));
		migrations.push_back(std::unique_ptr<Migration>(new SmartMigration("0.2", this,
			[](FuzeDBI::Connection* db, Mediaboard::State* state){std::println("This is the lambda and document_root is {}", state->getDocumentRoot().string()); })));
		return migrations;
	}
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

	// FuzeDBI::Connection* fuze_dbi;

	const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	// Board main_board;
	Board* main_board() { return &(this->boards.at(0)); }
	int createThread(int board_id, boost::json::object thread_json, int author_client_id) {
		return this->boards.at(board_id).createThread(thread_json, author_client_id);
	}
	int createMessage(int board_id, boost::json::object message_json, int author_client_id) {
		return this->boards.at(board_id).createMessage(message_json, author_client_id);
	}

	std::string dumpAllGroups(const std::optional<FuzeHttp::Client>& client) const {
		std::cout << "Dumping from ordered_groups_vec: ";

		boost::json::object groups_json;
		boost::json::array group_heirarchy_json;
		int group_editable_threshold;
		if (this->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS))
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
	// BasicResponse setGroupHeirarchy(const FuzeHttp::Client& client, std::vector<int> ordered_groups);
	// BasicResponse createAccount(nlohmann::json user_json);
	// std::string dumpMembersInGroup(int group_id) const;
	// std::string dumpMembersInGroupAsArray(int group_id) const;
	std::string dumpAllUsers(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::object users_json;
		int client_rank = this->getClientRank(client);
		bool client_has_manage_permissions_permission = this->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS);
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
	// std::string dumpPermissions(int client_id) const { return this->getPermissionCollectionsAsJson(client_id).dump(); }
	const Thread* getThread(int board_id, int thread_id) const { return this->boards.at(board_id).getThread(thread_id); }

	// bool usernameExists(std::string username) const { std::unordered_map<std::string, int>::const_iterator it = username_to_id_map.find(username); return it != username_to_id_map.end(); };
	// BasicResponse addUserToGroups(const FuzeHttp::Client& client, int user_id, std::vector<int> groups_by_id);

	// void websocketRead (FuzeHttp::WebsocketSession* session) override;
	void sendToThread (std::string message, int thread_id) {
		// Put the message in a shared pointer so we can re-use it for each client
		auto const ss = std::make_shared<std::string const>(std::move(message));

		// Make a local list of all the weak pointers representing
		// the sessions, so we can do the actual sending without
		// holding the mutex:
		std::vector<boost::weak_ptr<FuzeHttp::WebsocketSession>> v;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			v.reserve(websocket_sessions.size());
			this->main_board()->removeUnauthorizedListenersFromThread(thread_id);
			for(auto p : this->main_board()->getListenersFromThread(thread_id))
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

	// This mutex synchronizes all access to sessions_
	// std::mutex mutex_;


	// std::unordered_map<int, Board> boards;
	std::unordered_map<int, Mediaboard::Board> boards;
	// std::vector<int> ordered_boards;
	// HTTP sessions. Client validates using a cookie
}; // class State
} // namespace Mediaboard
