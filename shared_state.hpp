//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_SHARED_STATE_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_SHARED_STATE_HPP

#include "beast.hpp"
#include "Board.hpp"
#include "FuzeDBI.hpp"
#include "FuzeHttp.hpp"
#include "permission_managed_object.hpp"
#include <boost/filesystem.hpp>
#include <boost/smart_ptr.hpp>
#include <mutex>
#include <string>
#include <unordered_set>

// Forward declaration
class websocket_session;

struct StateConfig {
	std::string thumbnail_file_extension;
	unsigned int thumbnail_size;
	unsigned int max_http_body_in_megabytes;
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
class shared_state : public FuzeHttp::State {
public:
	shared_state(boost::filesystem::path document_root, boost::filesystem::path media_location_relative, StateConfig config, FuzeDBI::Connection* fuze_database_interface);
	const StateConfig config;
	void start();
	void setAdditionalImageFormatsFromConfig(const StateConfig& config);
	bool canCreateThumbnailForImageFormat(const std::string_view mime_type) const;
	bool canCreateThumbnailForVideoFormat(const std::string_view mime_type) const;

	// FuzeDBI::Connection* fuze_dbi;

	const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	// Board main_board;
	Board* main_board() { return &(this->boards.at(0)); }
	int createThread(int board_id, boost::json::object thread_json, int author_client_id);
	int createMessage(int board_id, boost::json::object message_json, int author_client_id);

	std::string dumpAllGroups(const std::optional<Client>& client) const;
	// BasicResponse setGroupHeirarchy(const Client& client, std::vector<int> ordered_groups);
	// BasicResponse createAccount(nlohmann::json user_json);
	// std::string dumpMembersInGroup(int group_id) const;
	// std::string dumpMembersInGroupAsArray(int group_id) const;
	std::string dumpAllUsers(const std::optional<Client>& client) const;
	// std::string dumpPermissions(int client_id) const { return this->getPermissionCollectionsAsJson(client_id).dump(); }
	const Thread* getThread(int board_id, int thread_id) const { return this->boards.at(board_id).getThread(thread_id); }
	std::string getIntermediateSaltFromAccount(int account_id);
	const Client& getClientFromAccountId(int account_id) const;
	// bool usernameExists(std::string username) const { std::unordered_map<std::string, int>::const_iterator it = username_to_id_map.find(username); return it != username_to_id_map.end(); };
	// BasicResponse addUserToGroups(const Client& client, int user_id, std::vector<int> groups_by_id);

	void join  (websocket_session* session);
	void leave (websocket_session* session);
	void sendToThread (std::string message, int thread_id);
	void sendToWebRTC(std::string message);
	void clearWebsockets();

	const boost::filesystem::path& getMediaLocation() const { return media_location; }
	// const boost::filesystem::path& getProgramLocation() const { return program_location; }
	const char* getSecret() const { return this->secret_base64; }
private:
	const boost::filesystem::path media_location;
	// const boost::filesystem::path program_location;
	char secret_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	FuzeDBI::Connection* fuze_dbi;
	std::unordered_set<std::string> image_formats_to_create_thumbnails_for = {"image/bmp", "image/gif", "image/vnd.microsoft.icon", "image/jpeg", "image/jxl", "image/png", "image/svg"};
	std::unordered_set<std::string> video_formats_to_create_thumbnails_for;

	// This mutex synchronizes all access to sessions_
	std::mutex mutex_;

	// Keep a list of all the websocket-connected clients
	std::unordered_set<websocket_session*> websocket_sessions;

	// std::unordered_map<int, Board> boards;
	std::unordered_map<int, Board> boards;
	// std::vector<int> ordered_boards;
	// HTTP sessions. Client validates using a cookie
};

#endif
