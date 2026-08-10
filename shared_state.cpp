//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "FuzeHttpServer.hpp"
#include "shared_state.hpp"
// #include "WebsocketSession.hpp"
#include <boost/json/serialize.hpp>
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/program_options.hpp>
#include <iostream>
import MediaboardWebsocketSession;
import Mediaboard.Board;

using namespace FuzeHttp;
using namespace Mediaboard;

// shared_state::shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates)
//		: State(fuze_database_interface, std::move(busted_target_to_target), std::move(files_generated_from_templates)),
shared_state::shared_state(FuzeHttp::Server* server, StateConfig config, bool create_owner_account)
		: State(server),
		config(config),
		// fuze_dbi(server->db),
		media_location(server->program_directories.media) {
	// std::println("Assigned document_root: {}", document_root.string());
	this->setAdditionalImageFormatsFromConfig(config);
	if (create_owner_account) {
		std::string invite_key = this->createInvite(static_cast<int>(BUILTIN_GROUPS::OWNER));
		std::cout << std::endl << "Use this link to register the owner account: http://localhost:" << this->server->server_port << "/invite/" << invite_key << std::endl;
	}
	else if (!this->ownerExists())
		std::println("\nERROR: No owner found. Restart the application with --create_owner");
	// this->document_root = document_root;

	// this->options.push_back({
	// 	new TemplateOption<std::string>("site_name", "Fuze Mediaboard", "Website name shown on tabs and headers."),
	// 	new TemplateOption<std::string>("favicon_url", "https://fuze.page/favicon.ico"),
	// 	new TemplateOption("show_watermarks", true)
	// 	new TemplateConstant("post_max_name", static_cast<int>(MESSAGE_FIELDS::MAX_NAME)),
	// 	new TemplateConstant("post_max_file_name", static_cast<int>(MESSAGE_FIELDS::MAX_FILE_NAME)),
	// 	new TemplateConstant("post_max_content", static_cast<int>(MESSAGE_FIELDS::MAX_CONTENT)),
	// 	new TemplateConstant("group_max_name", static_cast<int>(Group::MAX_NAME)),
	// 	new TemplateConstant("account_max_username", static_cast<int>(Account::MAX_USERNAME)),
	// 	new TemplateConstant("mediaboard_version", current_version)
	// });
}

void shared_state::setAdditionalImageFormatsFromConfig(const StateConfig& config) {
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

bool shared_state::canCreateThumbnailForImageFormat(const std::string_view mime_type) const {
	return this->image_formats_to_create_thumbnails_for.contains(std::string(mime_type));
}

bool shared_state::canCreateThumbnailForVideoFormat(const std::string_view mime_type) const {
	return this->video_formats_to_create_thumbnails_for.contains(std::string(mime_type));
}

// shared_from_this cannot be used in a constructor; see https://stackoverflow.com/questions/5558734/c-bad-weak-ptr-error
// hence a seperate start() function is used
// UPDATE 0.0.6: permission-managed objects no longer use shared pointers
void shared_state::start() {
	Mediaboard::Board main_board(this, db);
	this->boards.emplace(0, main_board);
	this->boards.at(0).cacheAllThreads();
}

// Broadcast a message to all websocket client sessions
void shared_state::sendToThread(std::string message, int thread_id) {
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

int shared_state::createThread(int board_id, boost::json::object thread_json, int author_client_id) {
	return this->boards.at(board_id).createThread(thread_json, author_client_id);
}

int shared_state::createMessage(int board_id, boost::json::object message_json, int author_client_id) {
	return this->boards.at(board_id).createMessage(message_json, author_client_id);
}

std::string shared_state::getIntermediateSaltFromAccount(int account_id) {
	return db->query<std::string>("SELECT intermediate_salt_base64 FROM account WHERE id = $1", account_id);
}

const Client& shared_state::getClientFromAccountId(int account_id) const { // We assume the account with the ID is already checked
	if (!this->accounts.at(account_id).client_id)
		throw std::runtime_error(std::format("[getClientFromAccountId] No client ID assigned to account {}", account_id));
	int client_id = this->accounts.at(account_id).client_id.value();
	auto it = this->clients.find(client_id);
	if (it == this->clients.end())
		throw std::runtime_error(std::format("Account {} refers to Client {} which does not exist", account_id, client_id));
	return it->second;
}

std::string shared_state::dumpAllGroups(const std::optional<Client>& client) const {
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

std::string shared_state::dumpAllUsers(const std::optional<Client>& client) const {
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
