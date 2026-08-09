#include "Thread.hpp"
#include "PermissionObject.hpp"
#include <boost/json/serialize.hpp>
// #include "WebsocketSession.hpp"
#include <string>
#include <iostream>
#include <cstring>
import MediaboardWebsocketSession;

Thread::Thread(PermissionObjectBase* permission_parent, FuzeDBI::Connection* fuze_dbi, int id, int permission_object_id)
		: PermissionManagedObject(permission_parent, permission_object_id, fuze_dbi),
		id(id),
		fuze_dbi(fuze_dbi) {
	this->cacheAllPermissions();
	this->thread_as_json = {
		{"id", id},
		{"reply_count", 0}
	};
}

// Save thread when JSON is received
Thread::Thread(PermissionObjectBase* permission_parent, boost::json::object thread_json, int author_client_id, FuzeDBI::Connection* fuze_dbi)
		: PermissionManagedObject(permission_parent, fuze_dbi),
		fuze_dbi(fuze_dbi) {
	boost::json::object post_zero = thread_json.at("post_zero").as_object();
	this->thread_as_json = thread_json;
	this->id = fuze_dbi->query<int>("SELECT thread_id FROM _sequences");
	fuze_dbi->query<void>("UPDATE _sequences SET thread_id = $1", this->id+1);
	fuze_dbi->query<void>("INSERT INTO thread(id, permission_object_id) VALUES ($1, $2)", this->id, this->getPermissionObjectId());
	this->thread_as_json["id"] = this->id;
	post_zero.emplace("thread_id", this->id);
	int new_message_id = this->createMessageFromJson(std::move(post_zero), author_client_id);
	this->thread_as_json["post_zero"] = this->messages.at(new_message_id).asJson();
	this->thread_as_json["reply_count"] = 0;
}

/*
// Cache thread using db_interface struct
Thread::Thread(PermissionObjectBase* permission_parent, struct db_thread_struct* thread_struct, FuzeDBI::Connection* fuze_dbi)
			: PermissionManagedObject(permission_parent, thread_struct->permission_object_id, fuze_dbi) {
	// this->cacheAllPermissions();
	this->id = thread_struct->id;
	this->deleted = thread_struct->deleted;
	// json thread_as_json;
	this->thread_as_json["id"] = this->id;
	this->thread_as_json["deleted"] = this->deleted;
	this->thread_as_json["reply_count"] = 0;
}
std::string Thread::dumpThread() const {
	return boost::json::serialize(this->thread_as_json);
}
*/

boost::json::object Thread::asJson(const std::optional<FuzeHttp::Client>& client) const {
	boost::json::object thread_json = this->thread_as_json;
	thread_json.emplace("client_permissions", this->getPermissionsAsJson(client));
	return thread_json;
}
boost::json::object Thread::asJsonWithMessages(const std::optional<FuzeHttp::Client>& client) const {
	boost::json::object thread_json = this->asJson(client);
	thread_json.emplace("messages", this->getMessagesAsJson());
	return thread_json;
}

/*
void Thread::createPostFromStruct(struct db_post_struct* post_struct) {
	Post post(post_struct);
	if (post.getIdInThread() == 0)
		this->thread_as_json["post_zero"] = post.asJson();
	else if (!post.isDeleted()) {
		reply_count++;
		this->thread_as_json["reply_count"] = this->reply_count;
	}
	this->messages.emplace(post.getIdInThread(), post);
	this->last_post_timestamp = post.getUploadTimestamp();
}
*/
void Thread::cacheMessage(Message&& message) {
	if (message.getIdInThread() == 0)
		this->thread_as_json["post_zero"] = message.asJson();
	else if (!message.isDeleted()) {
		this->reply_count++;
		this->thread_as_json["reply_count"] = this->reply_count;
	}
	this->messages.emplace(message.getIdInThread(), message);
	this->last_message_created_at = message.createdAt();
}

int Thread::createMessageFromJson(boost::json::object post_json, int author_client_id) {
	int new_message_id_in_thread = fuze_dbi->query<int>("SELECT message_id_seq FROM thread WHERE id = $1", this->id);
	fuze_dbi->query<void>("UPDATE thread SET message_id_seq = $1 WHERE id = $2", new_message_id_in_thread+1, this->id);
	post_json.emplace("id_in_thread", (size_t)new_message_id_in_thread);
	// const std::string placeholder_key(KEY_LENGTH+1, 'T');
	// post_json["key"] = placeholder_key;
	Message message(post_json, author_client_id, fuze_dbi); // Key is deleted from post_json in its constructor
	if (this->messages.empty())
		this->thread_as_json["post_zero"] = message.asJson();
	else {
		this->reply_count++;
		this->thread_as_json["reply_count"] = this->reply_count;
	}
	this->messages.emplace(message.getIdInThread(), message);
	this->last_message_created_at = message.createdAt();
	return message.getIdInThread();
}

void Thread::deleteMessage(int id_in_thread) {
	this->messages.at(id_in_thread).markAsDeleted();
	fuze_dbi->query<void>("UPDATE message SET deleted = TRUE WHERE thread_id = $1 AND id_in_thread = $2", this->id, id_in_thread);
	this->reply_count--;
	this->thread_as_json["reply_count"] = this->reply_count;
	std::cout << "Erased message " << id_in_thread << " from thread " << this->id << std::endl;
}

// bool Thread::keyMatchesMessage(std::string key, int message_id) const {
// 	// std::cout << "[Thread] key :: message_key\n" << key << " :: " << this->messages.at(message_id).getKey() << std::endl;
// 	return message_id != 0 && this->messages.at(message_id).getKey() == key; // Don't match key to message 0, because threads can't be deleted by regular users (yet?)
// }

boost::json::array Thread::getMessagesAsJson() const {
	boost::json::array multiple_post_json = boost::json::array();
	for (auto it = this->messages.begin(); it != this->messages.end(); ++it) {
		if (!it->second.isDeleted()) {
			// std::cout << "Dumping post " << this->id << "/" << it->second.getIdInThread() << std::endl;
			boost::json::object post_json = it->second.asJson();
			// post_json["is_author"] = keyMatchesMessage(key, it->first);
			multiple_post_json.push_back(post_json);
		}
	}
	return multiple_post_json;
}

std::string Thread::dumpMessage(int message_id) const {
	boost::json::object message_json = this->messages.at(message_id).asJson();
	return boost::json::serialize(message_json);
}

// std::string Thread::dumpPermissions(int client_id) const {
// 	return this->getPermissionCollectionsAsJson(client_id).dump();
// 	// return "NOT IMPLEMENTED";
// }

void Thread::markAsDeleted() {
	this->deleted = true;
	fuze_dbi->query<void>("UPDATE thread SET deleted = TRUE WHERE id = $1", this->id);
	fuze_dbi->query<void>("UPDATE message SET deleted = TRUE WHERE thread_id = $1", this->id);
}

void Thread::addListener(FuzeHttp::WebsocketSession* listener) {
	listeners.insert(listener);
}

void Thread::removeListener(FuzeHttp::WebsocketSession* listener) {
	listeners.erase(listener);
}

boost::json::object Thread::getPermissionsAsJson(const std::optional<FuzeHttp::Client>& client) const {
	// json permissions_as_json;
	// permissions_as_json["manage_permissions"] = this->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS);
	// permissions_as_json["send_message"] = this->clientHasPermission(client, PERMISSION::SEND_MESSAGE);
	// permissions_as_json["delete_post"] = this->clientHasPermission(client, PERMISSION::DELETE_POST);
	return {
		{"manage_permissions", this->clientHasPermission(client, FuzeHttp::PERMISSION::MANAGE_PERMISSIONS)},
		{"send_message", this->clientHasPermission(client, FuzeHttp::PERMISSION::SEND_MESSAGE)},
		{"delete_post", this->clientHasPermission(client, FuzeHttp::PERMISSION::DELETE_POST)},
		{"upload_file", this->clientHasPermission(client, FuzeHttp::PERMISSION::UPLOAD_FILE)},
	};
}

std::unordered_set<FuzeHttp::WebsocketSession*> Thread::getListeners() const {
	return this->listeners;
};

void Thread::removeUnauthorizedListeners() {
	std::erase_if(this->listeners, [this](const FuzeHttp::WebsocketSession* ws)->bool{
		return !this->clientHasPermission(ws->getClient(), FuzeHttp::PERMISSION::VIEW_THREAD);
	});
}
