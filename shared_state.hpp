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
#include "board.hpp"
#include "DatabaseConnection.hpp"
#include "FuzeDBI.hpp"
#include "permission_managed_object.hpp"
#include <boost/filesystem.hpp>
#include <boost/smart_ptr.hpp>
#include <mutex>
#include <string>
#include <unordered_set>

// Forward declaration
class websocket_session;

// using json = nlohmann::json;

class BasicResponse {
public:
	BasicResponse(http::status status, std::string message) :
	   	status(status), message(message) {}
	BasicResponse(http::status status, nlohmann::json json) :
	   	status(status), json(json) {}
	http::status status;
	std::string message;
	boost::optional<nlohmann::json> json;
};

struct Session {
	int account_id;
	std::chrono::time_point<std::chrono::system_clock> created_at;
};

struct Invite {
	int granted_group_id;
	std::chrono::time_point<std::chrono::system_clock> created_at;
};

// Represents the shared server state
class shared_state : public PermissionManager {
public:
	shared_state(boost::filesystem::path parent_directory, boost::filesystem::path media_location_relative, DatabaseConnection* database_connection, std::string thumbnail_file_format, FuzeDBI* fuze_database_interface);
	~shared_state();
	void start();

	DatabaseConnection* db;
	FuzeDBI* fuze_dbi;

	const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	// Board main_board;
	Board* main_board() { return &(this->boards.at(0)); }

	std::string dumpAllGroups(int client_id) const;
	BasicResponse setGroupHeirarchy(int client_id, std::vector<int> ordered_groups);
	BasicResponse createAccount(nlohmann::json user_json);
	std::string dumpMembersInGroup(int group_id) const;
	std::string dumpMembersInGroupAsArray(int group_id) const;
	std::string dumpAllUsers(int client_id) const;
	std::string dumpPermissions(int client_id) const { return this->getPermissionCollectionsAsJson(client_id).dump(); }
	boost::shared_ptr<Thread> getThread(int board_id, int thread_id) const { return this->boards.at(board_id).getThread(thread_id); }
	// bool usernameExists(std::string username) const { std::unordered_map<std::string, int>::const_iterator it = username_to_id_map.find(username); return it != username_to_id_map.end(); };
	BasicResponse getKeyFromPassword(nlohmann::json request_json) const;
	BasicResponse addUserToGroups(int client_id, int user_id, std::vector<int> groups_by_id);

	void join  (websocket_session* session);
	void leave (websocket_session* session);
	void sendToThread (std::string message, int thread_id);

	const boost::filesystem::path& getMediaLocation() const { return media_location; }
	const boost::filesystem::path& getProgramLocation() const { return program_location; }
	const std::string& getThumbnailFileFormat() const { return thumbnail_file_format; }
	const char* getSecret() const { return this->secret_base64; }

	std::string createSession(int account_id);
	int getClientIdFromSession(const std::string& session_id_base64) const;
	void clearExpiredSessions();

	// void createOwnerAccount(DatabaseConnection* db, const std::string& username, const std::string& password);
	std::string createInvite(int granted_group_id = static_cast<int>(BUILTIN_GROUPS::USERS));
	int getGrantedGroupIdFromInvite(const std::string& invite_key_base64) const; // returns PUBLIC if none found
private:
	const boost::filesystem::path media_location;
	const boost::filesystem::path program_location;
	const std::string thumbnail_file_format;
	char secret_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];

	// This mutex synchronizes all access to sessions_
	std::mutex mutex_;

	// Keep a list of all the websocket-connected clients
	std::unordered_set<websocket_session*> sessions_;

	// std::unordered_map<int, Board> boards;
	std::unordered_map<int, Board> boards;
	// std::vector<int> ordered_boards;
	const std::chrono::duration<unsigned int> authorization_token_lifespan = std::chrono::days(365);
	// HTTP sessions. Client validates using a cookie
	std::unordered_map<std::string /*key_base64*/, Session> sessions;
	std::unordered_map<std::string /*key_base64*/, Invite> invites;
};

#endif
