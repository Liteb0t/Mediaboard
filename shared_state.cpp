//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "permission_managed_object.hpp"
#include "shared_state.hpp"
#include "websocket_session.hpp"
#include <iostream>

shared_state::shared_state(boost::filesystem::path parent_directory, boost::filesystem::path media_location, DatabaseConnection* db, std::string thumbnail_file_format)
		: PermissionManager(0, db),
		program_location(std::move(parent_directory)),
		media_location(std::move(media_location)),
		db(db),
		thumbnail_file_format(thumbnail_file_format) {
}

// shared_from_this cannot be used in a constructor; see https://stackoverflow.com/questions/5558734/c-bad-weak-ptr-error
// hence a seperate start() function is used
// UPDATE 0.0.6: permission-managed objects no longer use shared pointers
void shared_state::start() {
	Board main_board(this, db);
	this->boards.emplace(0, main_board);
	this->boards.at(0).cacheAllThreads();
}

void shared_state::join(websocket_session* session) {
	std::lock_guard<std::mutex> lock(mutex_);
	sessions_.insert(session);
}

void shared_state::leave(websocket_session* session) {
	std::lock_guard<std::mutex> lock(mutex_);
	sessions_.erase(session);
}

// Broadcast a message to all websocket client sessions
void shared_state::sendToThread(std::string message, int thread_id) {
	// Put the message in a shared pointer so we can re-use it for each client
	auto const ss = boost::make_shared<std::string const>(std::move(message));

	// Make a local list of all the weak pointers representing
	// the sessions, so we can do the actual sending without
	// holding the mutex:
	std::vector<boost::weak_ptr<websocket_session>> v;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		v.reserve(sessions_.size());
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

std::string shared_state::dumpAllGroups(int client_id) const {
	std::cout << "Dumping from ordered_groups_vec: ";

	json groups_json;
	groups_json["groups"] = json::object();
	int group_editable_threshold;
	if (this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS))
		group_editable_threshold = this->getUserRank(client_id) + 1;
	else
		group_editable_threshold = this->getOrderedGroups()->size();
	for (int i = 0; i < this->getOrderedGroups()->size(); i++) {
	// for (int group_id : *(this->getOrderedGroups())) {
		int group_id = (*(this->getOrderedGroups()))[i];
		std::cout << group_id << ", ";
		json group_json = this->getGroup(group_id)->asJson();
		if (i >= group_editable_threshold) {
			group_json["heirarchy_editable"] = true;
			group_json["permission_editable"] = true;
		}
		else {
			group_json["heirarchy_editable"] = false;
			group_json["permission_editable"] = false;
		}
		/*if (group_id == static_cast<int>(BUILTIN_GROUPS::ADMINISTRATORS))
			group_json["lock_position"] = "top";
		else*/ 
		if (	   group_id == static_cast<int>(BUILTIN_GROUPS::USERS)
				|| group_id == static_cast<int>(BUILTIN_GROUPS::PUBLIC))
			group_json["heirarchy_editable"] = false;
		groups_json["groups"][std::to_string(group_id)] = group_json;
		// get user from key
		// if group rank >= user rank (measured by the highest group the user is in), then
			// group is locked to the top

	}
	std::cout << " done." << std::endl;

	groups_json["group_heirarchy"] = *(this->getOrderedGroups());
	return groups_json.dump();
}

std::string shared_state::dumpMembersInGroup(int group_id) const {
	const Group* group = this->getGroup(group_id);
	const std::unordered_set<int> members = group->getMembers();
	nlohmann::json members_json;
	members_json["members"] = nlohmann::json::object();
	for (int member_id : members) {
		std::string member_id_as_string = std::to_string(member_id);
		members_json["members"][member_id_as_string] = nlohmann::json::object();
		const User* user = this->getUser(member_id);
		members_json["members"][member_id_as_string]["id"] = member_id;
		members_json["members"][member_id_as_string]["username"] = user->getUsername();
	}
	return members_json.dump();
}

std::string shared_state::dumpMembersInGroupAsArray(int group_id) const {
	nlohmann::json members_json = this->getGroup(group_id)->getMembers();
	return members_json.dump();
}

// Return non-zero when action is rejected. An error is returned to the user from http_session
BasicResponse shared_state::setGroupHeirarchy(int client_id, std::vector<int> ordered_groups) {
	int user_rank;
	if (!this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS))
		return BasicResponse(http::status::bad_request, std::string("Cannot change group heirarchy; permission denied."));
	else
		user_rank = this->getUserRank(client_id);

	if (ordered_groups.size() != this->getOrderedGroups()->size()) {
		return BasicResponse(http::status::bad_request, std::string("Number of groups does not match."));
	}

	std::unordered_set<int> new_group_order_set;
	// for (const int group_id : *(this->getOrderedGroups())) {
	for (int group_rank = 0; group_rank < ordered_groups.size(); group_rank++) {
		int group_id = ordered_groups[group_rank];
		std::cout << group_id << "G : ";
		// Check for duplicates
		std::unordered_set<int>::const_iterator duplicate_check_it = new_group_order_set.find(group_id); 
		if (duplicate_check_it != new_group_order_set.end())
			return BasicResponse(http::status::bad_request, "Duplicate group " + std::to_string(group_id) + " detected");
		else
			new_group_order_set.insert(group_id);

		// Check if all groups exist
		if (!this->groupExists(group_id)) {
			return BasicResponse(http::status::bad_request, "Group " + std::to_string(group_id) + " does not exist."); // Group does not exist
		}
		// Check if user rank is high enough to change this group's rank
		int existing_group_at_this_rank = (*(this->getOrderedGroups()))[group_rank];
		std::cout << existing_group_at_this_rank << std::endl;
		if (group_rank <= user_rank && group_id != existing_group_at_this_rank)
			return BasicResponse(http::status::bad_request, std::string("Permission denied; attempted to change order of groups greater than or equal to your rank.") /*" group_rank: " + std::to_string(group_rank) + ", user_rank: " + std::to_string(user_rank)*/);
	}
	if (ordered_groups[0] != static_cast<int>(BUILTIN_GROUPS::ADMINISTRATORS) ||
			ordered_groups[ordered_groups.size()-2] != static_cast<int>(BUILTIN_GROUPS::USERS) ||
			ordered_groups[ordered_groups.size()-1] != static_cast<int>(BUILTIN_GROUPS::PUBLIC)) {
		return BasicResponse(http::status::bad_request, std::string("Attempted to change heirarchy of locked groups"));
	}

	// this->ordered_groups_vec = ordered_groups;
	this->setOrderedGroups(ordered_groups);

	return BasicResponse(http::status::ok, std::string("Updated group heirarchy")); // Success
}

BasicResponse shared_state::createAccount(json account_json) {
	if (	   account_json.contains("username")
			&& account_json.contains("password")
			) {
		std::string username = account_json["username"].template get<std::string>();
		if (username == "Administrator")
			return BasicResponse(http::status::bad_request, std::string("Username reserved"));
		else if (db_account_username_exists(username.c_str()))
			return BasicResponse(http::status::bad_request, std::string("Username already exists"));

		std::string password = account_json["password"].template get<std::string>();
		if (password.length() < ACCOUNT_MIN_PASSWORD)
			return BasicResponse(http::status::bad_request, "Password must contain at least " + std::to_string(ACCOUNT_MIN_PASSWORD) + " character(s)");
		
		const User* new_user = this->createUser(username, password);
		json response_json;
		// response_json["account_id"] = new_user->getId();
		response_json["account_key"] = new_user->getKey();
		return BasicResponse(http::status::created, response_json);
	}
	else
		return BasicResponse(http::status::bad_request, std::string("One or more JSON fields missing from request"));
}

BasicResponse shared_state::getKeyFromPassword(json request_json) const {
	if (	   request_json.contains("username")
			&& request_json.contains("password")
			) {
		std::string username = request_json["username"].template get<std::string>();
		if (this->userExists(username)) {
			// const User* user = this->getUser(username);
			int user_id = this->getIdFromUsername(username);
			std::string password = request_json["password"].template get<std::string>();
			if (this->checkUserPassword(user_id, password)) {
				// Password is correct
				json response_json;
				response_json["account_key"] = this->getUserKey(user_id);
				return BasicResponse(http::status::ok, response_json);
			}
			else
				return BasicResponse(http::status::bad_request, std::string("The password is incorrect"));
		}
		else
			return BasicResponse(http::status::bad_request, std::string("No user with that username was found"));
	}
	else
		return BasicResponse(http::status::bad_request, std::string("One or more JSON fields missing from request"));
}

std::string shared_state::dumpAllUsers(int client_id) const {
	json users_json;
	users_json["users"] = json::object();
	int client_rank = this->getUserRank(client_id);
	bool client_has_manage_permissions_permission = this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS);
	boost::shared_ptr<std::unordered_map<int, User>> _users = this->getUsers();
	for (std::unordered_map<int, User>::const_iterator user_it = _users->begin(); user_it != _users->end(); user_it++) {
		int user_id = user_it->first;
		const User* user = this->getUser(user_id);
		std::cout << user_id << ", ";
		// json user_json = this->getUser(user_id)->asJson();
		nlohmann::json user_json = json::object();
		user_json["id"] = user_id;
		user_json["username"] = user->getUsername();
		int user_rank = this->getUserRank(user_id);
		user_json["rank"] = user_rank;
		user_json["groups"] = json::array();
		// int group_rank = 0;
		for (const int group_id : this->getOrderedGroupsContainingMember(user_id)) {
			const Group* group = this->getGroup(group_id);
			nlohmann::json group_json;
			group_json["id"] = group->getId();
			group_json["name"] = group->getName();
			user_json["groups"].push_back(group_json);
		}
		user_json["permission_editable"] = client_has_manage_permissions_permission && client_rank < user_rank;
		users_json["users"][std::to_string(user_id)] = user_json;
	}
	std::cout << " done." << std::endl;

	return users_json.dump();
}

BasicResponse shared_state::addUserToGroups(int client_id, int user_id, std::vector<int> groups_by_id) {
	int client_rank;
	if (!this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS))
		return BasicResponse(http::status::forbidden, std::string("Cannot change group heirarchy; permission denied."));
	else
		client_rank = this->getUserRank(client_id);
	std::cout << "User has permission. ";
	for (int group_id : groups_by_id) {
		if (	static_cast<BUILTIN_GROUPS>(group_id) == BUILTIN_GROUPS::USERS
			||  static_cast<BUILTIN_GROUPS>(group_id) == BUILTIN_GROUPS::PUBLIC) {
			return BasicResponse(http::status::bad_request, std::string("Attempted to add user to one or more groups to which no user can be added, namely, the \"USERS\" and \"PUBLIC\" groups."));
		}
	}
	for (int group_id : groups_by_id) {
		// const Group* group = this->getGroup(group_id);
		if (client_rank < this->getGroupRank(group_id)) {
			this->addUserToGroup(user_id, group_id);
		}
		else {
			return BasicResponse(http::status::forbidden, std::string("Permission denied; Attempted to add user to group with a rank greater than or equal to your own."));
		}
	}
	return BasicResponse(http::status::ok, std::string("Added user to groups")); // Success
}

