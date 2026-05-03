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
#include <boost/json/serialize.hpp>
#include <iostream>

shared_state::shared_state(boost::filesystem::path document_root, boost::filesystem::path media_location, std::string thumbnail_file_format, FuzeDBI::Connection* fuze_database_interface)
		: PermissionManager(0, fuze_database_interface),
		State(fuze_database_interface),
		fuze_dbi(fuze_database_interface),
		// document_root(std::move(document_root)),
		media_location(std::move(media_location)),
		thumbnail_file_format(thumbnail_file_format) {
	// db->getSecret(this->secret_base64);
	this->document_root = document_root;
	for (const auto& client_pair : this->clients) {
		if (client_pair.second.account_id) {
			int account_id = client_pair.second.account_id.value();
			auto it = this->accounts.find(account_id);
			if (it == this->accounts.end())
				throw std::runtime_error(std::format("Client {} refers to account {} which does not exist", client_pair.first, account_id));
			it->second.client_id = client_pair.first;
			std::cout << "Account " <<account_id << " = Client " <<client_pair.first << std::endl;
		}
	}
	/* FuzeDBI demo
	fuze_dbi->query<void>("INSERT INTO _info(version) VALUES ($1)", "cocks");
	auto version = fuze_dbi->query<std::string>("SELECT (version) FROM _info");
	std::cout << "[shared_state] version: " <<version << std::endl;
	auto toople = fuze_dbi->query<std::tuple<int, std::string>>("SELECT id, username FROM account");
	std::cout << "id: " << std::get<0>(toople) << ", username: " << std::get<1>(toople) << std::endl;
	for (auto row : fuze_dbi->queryRows<std::tuple<int, int>>("SELECT permission_number, setting FROM permission_setting")) {
		std::cout << std::get<0>(row) << '_' << std::get<1>(row) << std::endl;
	}
	*/
}

// shared_from_this cannot be used in a constructor; see https://stackoverflow.com/questions/5558734/c-bad-weak-ptr-error
// hence a seperate start() function is used
// UPDATE 0.0.6: permission-managed objects no longer use shared pointers
void shared_state::start() {
	Board main_board(this, fuze_dbi);
	this->boards.emplace(0, main_board);
	this->boards.at(0).cacheAllThreads();
}

void shared_state::join(websocket_session* session) {
	std::lock_guard<std::mutex> lock(mutex_);
	websocket_sessions.insert(session);
}

void shared_state::leave(websocket_session* session) {
	std::lock_guard<std::mutex> lock(mutex_);
	websocket_sessions.erase(session);
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
		v.reserve(websocket_sessions.size());
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

std::string shared_state::getIntermediateSaltFromAccount(int account_id) {
	return fuze_dbi->query<std::string>("SELECT intermediate_salt_base64 FROM account WHERE id = $1", account_id);
}

const FuzeHttp::Client& shared_state::getClientFromAccountId(int account_id) const { // We assume the account with the ID is already checked
	if (!this->accounts.at(account_id).client_id)
		throw std::runtime_error(std::format("[getClientFromAccountId] No client ID assigned to account {}", account_id));
	int client_id = this->accounts.at(account_id).client_id.value();
	auto it = this->clients.find(client_id);
	if (it == this->clients.end())
		throw std::runtime_error(std::format("Account {} refers to Client {} which does not exist", account_id, client_id));
	return it->second;
}

std::string shared_state::dumpAllGroups(const std::optional<FuzeHttp::Client>& client) const {
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
			{"heirarchy_editable", i >= group_editable_threshold && (group_id == static_cast<int>(BUILTIN_GROUPS::USERS) || group_id == static_cast<int>(BUILTIN_GROUPS::PUBLIC))},
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
/*
std::string shared_state::dumpMembersInGroup(int group_id) const {
	const std::unordered_set<int> members = this->getGroup(group_id)->getMembers();
	boost::json::object members_json;
	for (int member_id : members) {
		boost::json::object member_json{
			{"id", member_id},
			{"username", this->getUsernameFromAccount(member_id)}
		};
		members_json.emplace(std::to_string(member_id), member_json);
	}
	return boost::json::serialize(boost::json::object{
		{"members", members_json},
	});
}

std::string shared_state::dumpMembersInGroupAsArray(int group_id) const {
	nlohmann::json members_json = this->getGroup(group_id)->getMembers();
	return members_json.dump();
}

// Return non-zero when action is rejected. An error is returned to the user from http_session
BasicResponse shared_state::setGroupHeirarchy(const FuzeHttp::Client& client, std::vector<int> ordered_groups) {
	int user_rank;
	if (!this->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS))
		return BasicResponse(http::status::bad_request, std::string("Cannot change group heirarchy; permission denied."));
	else
		user_rank = this->getClientRank(client);

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
			return BasicResponse(http::status::bad_request, std::string("Permission denied; attempted to change order of groups greater than or equal to your rank.") );
	}
	if (ordered_groups[0] != static_cast<int>(BUILTIN_GROUPS::OWNER) ||
			ordered_groups[ordered_groups.size()-2] != static_cast<int>(BUILTIN_GROUPS::USERS) ||
			ordered_groups[ordered_groups.size()-1] != static_cast<int>(BUILTIN_GROUPS::PUBLIC)) {
		return BasicResponse(http::status::bad_request, std::string("Attempted to change heirarchy of locked groups"));
	}

	// this->ordered_groups_vec = ordered_groups;
	this->setOrderedGroups(ordered_groups);

	return BasicResponse(http::status::ok, std::string("Updated group heirarchy")); // Success
}
*/

std::string shared_state::dumpAllUsers(const std::optional<FuzeHttp::Client>& client) const {
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
			{"rank", client_rank}
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
/*
BasicResponse shared_state::addUserToGroups(const FuzeHttp::Client& client, int account_id, std::vector<int> groups_by_id) {
	int client_rank;
	if (!this->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS))
		return BasicResponse(http::status::forbidden, std::string("Cannot change group heirarchy; permission denied."));
	else
		client_rank = this->getClientRank(client);
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
			this->addAccountToGroup(account_id, group_id);
		}
		else {
			return BasicResponse(http::status::forbidden, std::string("Permission denied; Attempted to add user to group with a rank greater than or equal to your own."));
		}
	}
	return BasicResponse(http::status::ok, std::string("Added user to groups")); // Success
}
*/
/* I was unable to generate a key here that would work with the frontend WASM module.
void shared_state::createOwnerAccount(DatabaseConnection* db, const std::string& username, const std::string& password) {
	unsigned char intermediate_salt[crypto_pwhash_SALTBYTES];
	randombytes_buf(intermediate_salt, crypto_pwhash_SALTBYTES);
	char intermediate_salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(
		intermediate_salt_base64, sizeof intermediate_salt_base64,
		intermediate_salt, crypto_pwhash_SALTBYTES,
		sodium_base64_VARIANT_URLSAFE
	);
	std::string intermediate_salt_base64_str = intermediate_salt_base64;
	unsigned char salt[crypto_pwhash_SALTBYTES];
	crypto_generichash(
		salt, crypto_pwhash_SALTBYTES,
		reinterpret_cast<const unsigned char*>(username.c_str()), username.length(),
		reinterpret_cast<const unsigned char*>(intermediate_salt_base64_str.c_str()), intermediate_salt_base64_str.length()
	);
	char salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(
		salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE),
		salt, crypto_pwhash_SALTBYTES,
		sodium_base64_VARIANT_URLSAFE
	);

	unsigned char password_hash[crypto_pwhash_STRBYTES];
	std::cout <<
		"Password: " <<password <<
		"\nintermediate_salt_base64: " << intermediate_salt_base64_str <<
		"\nsalt_base64: " <<salt_base64 <<
		"\nlimits: " << this->client_pwhash_opslimit << ", " << (this->client_pwhash_memlimit >> 10) << std::endl;
	int res = crypto_pwhash(
		password_hash, sizeof password_hash,
		password.c_str(), password.length(),
		reinterpret_cast<const unsigned char*>(salt_base64),
		this->client_pwhash_opslimit,
		this->client_pwhash_memlimit, crypto_pwhash_ALG_ARGON2ID13
	);

	std::cout << "[shared_state] password_hash: " << password_hash << std::endl;

	char password_hash_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_STRBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(
		password_hash_base64, sizeof password_hash_base64,
		password_hash, sizeof password_hash,
		sodium_base64_VARIANT_URLSAFE
	);
	std::cout << "[shared_state] password_hash_base64: " << password_hash_base64 << std::endl;

	char password_hash_hash_base64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE)];
	FuzeHttp::generatePasswordHashHashBase64(
		password_hash_hash_base64, sizeof password_hash_hash_base64,
		password_hash_base64, sizeof password_hash_base64
	);

	int user_id = db->createAccount(username, std::move(password_hash_hash_base64), intermediate_salt_base64);
	std::cout << "Created owner account " << username << std::endl;
	// std::string session_id_base64 = state->addSession(user_id);
}
*/
