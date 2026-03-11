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

#include <boost/smart_ptr.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include "beast.hpp"
#include "board.hpp"
#include "permission_managed_object.hpp"

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

// Represents the shared server state
class shared_state : public PermissionManager {
public:
	explicit
	shared_state(std::string parent_directory, std::string doc_root /*, std::string media_root*/);
	void start();

	const std::string parent_directory;

	const std::string& doc_root() const noexcept { return doc_root_; }
	// const std::string& media_root() const { return media_root_; }

	// Board main_board;
	boost::shared_ptr<Board> main_board() const { return this->boards.at(0); }

	std::string dumpAllGroups(int client_id) const;
	BasicResponse setGroupHeirarchy(int client_id, std::vector<int> ordered_groups);
	BasicResponse createAccount(nlohmann::json user_json);
	std::string dumpMembersInGroup(int group_id) const;
	std::string dumpMembersInGroupAsArray(int group_id) const;
	std::string dumpAllUsers(int client_id) const;
	std::string dumpPermissions(int client_id) const { return this->getPermissionCollectionsAsJson(client_id).dump(); }
	boost::shared_ptr<Thread> getThread(int board_id, int thread_id) const { return this->main_board()->getThread(thread_id); }
	// bool usernameExists(std::string username) const { std::unordered_map<std::string, int>::const_iterator it = username_to_id_map.find(username); return it != username_to_id_map.end(); };
	BasicResponse getKeyFromPassword(nlohmann::json request_json) const;
	BasicResponse addUserToGroups(int client_id, int user_id, std::vector<int> groups_by_id);

	void join  (websocket_session* session);
	void leave (websocket_session* session);
	void send  (std::string message);
	void sendToThread (std::string message, int thread_id);

private:
	const std::string doc_root_;
	// const std::string media_root_;

	// This mutex synchronizes all access to sessions_
	std::mutex mutex_;

	// Keep a list of all the connected clients
	std::unordered_set<websocket_session*> sessions_;

	// std::unordered_map<int, Board> boards;
	std::unordered_map<int, boost::shared_ptr<Board>> boards;
	// std::vector<int> ordered_boards;
};

#endif
