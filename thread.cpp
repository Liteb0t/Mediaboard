#include "thread.hpp"
#include <string>
#include <iostream>
#include <cstring>

// Save thread when JSON is received
Thread::Thread(PermissionObjectBase* permission_parent, json thread_json, DatabaseConnection* db)
			: PermissionManagedObject(permission_parent, db),
			db(db) {
	// thread_json.erase("key");
	this->thread_as_json = thread_json;
	// ID and timestamp are not initially known
	this->id = db_store_thread(this->permission_object_id);
	std::cout << "this->id: " << this->id << std::endl;
	this->thread_as_json["id"] = this->id;
	thread_json["post_zero"]["thread_id"] = this->id;
	this->createPostFromJson(thread_json["post_zero"]);
	this->thread_as_json["reply_count"] = 0;
}

// Cache thread using db_interface struct
Thread::Thread(PermissionObjectBase* permission_parent, struct db_thread_struct* thread_struct, DatabaseConnection* db)
			: PermissionManagedObject(permission_parent, thread_struct->permission_object_id, db),
			db(db) {
	// this->cacheAllPermissions();
	this->id = thread_struct->id;
	this->deleted = thread_struct->deleted;
	// json thread_as_json;
	this->thread_as_json["id"] = this->id;
	this->thread_as_json["deleted"] = this->deleted;
	this->thread_as_json["reply_count"] = 0;
}

std::string Thread::dumpThread() const {
	return this->thread_as_json.dump();
}

void Thread::createPostFromStruct(struct db_post_struct* post_struct) {
	Post post(post_struct, db);
	if (post.getIdInThread() == 0)
		this->thread_as_json["post_zero"] = post.asJson();
	else if (!post.isDeleted()) {
		reply_count++;
		this->thread_as_json["reply_count"] = this->reply_count;
	}
	this->posts.emplace(post.getIdInThread(), post);
	this->last_post_timestamp = post.getUploadTimestamp();
}

int Thread::createPostFromJson(json post_json) {
	post_json["id_in_thread"] = this->posts.size();
	// const std::string placeholder_key(KEY_LENGTH+1, 'T');
	// post_json["key"] = placeholder_key;
	Post post(post_json, db); // Key is deleted from post_json in its constructor
	if (this->posts.empty())
		this->thread_as_json["post_zero"] = post.asJson();
	else {
		this->reply_count++;
		this->thread_as_json["reply_count"] = this->reply_count;
	}
	this->posts.emplace(post.getIdInThread(), post);
	this->last_post_timestamp = post.getUploadTimestamp();
	return post.getIdInThread();
}

void Thread::deleteMessage(int message_id) {
	this->posts.at(message_id).markAsDeleted();
	this->reply_count--;
	this->thread_as_json["reply_count"] = this->reply_count;
	std::cout << "Erased message " << message_id << " from thread " << this->id << std::endl;
}

bool Thread::keyMatchesMessage(std::string key, int message_id) const {
	// std::cout << "[Thread] key :: message_key\n" << key << " :: " << this->posts.at(message_id).getKey() << std::endl;
	return message_id != 0 && this->posts.at(message_id).getKey() == key; // Don't match key to message 0, because threads can't be deleted by regular users (yet?)
}

nlohmann::json Thread::getMessagesAsJson(std::string key) const {
	json multiple_post_json = json::array();
	for (auto it = this->posts.begin(); it != this->posts.end(); ++it) {
		if (!it->second.isDeleted()) {
			// std::cout << "Dumping post " << this->id << "/" << it->second.getIdInThread() << std::endl;
			json post_json = it->second.asJson();
			post_json["is_author"] = keyMatchesMessage(key, it->first);
			multiple_post_json.push_back(post_json);
		}
	}
	return multiple_post_json;
}

std::string Thread::dumpPost(int message_id, std::string key) const {
	json post_json = this->posts.at(message_id).asJson();
	post_json["is_author"] = keyMatchesMessage(key, message_id);
	return post_json.dump();
}

std::string Thread::dumpPermissions(int client_id) const {
	return this->getPermissionCollectionsAsJson(client_id).dump();
	// return "NOT IMPLEMENTED";
}

void Thread::markAsDeleted() {
	this->deleted = true;
	db_mark_thread_as_deleted(this->id);
}

void Thread::addListener(websocket_session* listener) {
	listeners.insert(listener);
}

void Thread::removeListener(websocket_session* listener) {
	listeners.erase(listener);
}

json Thread::getPermissionsAsJson(int client_id) const {
	json permissions_as_json;
	permissions_as_json["manage_permissions"] = this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS);
	permissions_as_json["send_message"] = this->userHasPermission(client_id, PERMISSION::SEND_MESSAGE);
	permissions_as_json["delete_post"] = this->userHasPermission(client_id, PERMISSION::DELETE_POST);
	return permissions_as_json;
}
