#include "thread.hpp"
#include <string>
#include <iostream>
#include <cstring>

// Save thread when JSON is received
Thread::Thread(boost::shared_ptr<PermissionObjectBase> permission_parent, json thread_json, int new_permission_object_id)
			: PermissionManagedObject(permission_parent, new_permission_object_id) {
	// thread_json.erase("key");
	this->thread_as_json = thread_json;
	this->number_of_posts = 0;
	// if (save_to_database) {
		// ID and timestamp are not initially known
		this->id = db_store_thread(1, new_permission_object_id);
		std::cout << "this->id: " << this->id << std::endl;
		this->thread_as_json["id"] = this->id;
		thread_json["post_zero"]["thread_id"] = this->id;
		this->createPostFromJson(thread_json["post_zero"]);
	// }
	// else {
	// 	this->id = thread_json["id"].template get<int>();
	// }
}

// Cache thread using db_interface struct
Thread::Thread(boost::shared_ptr<PermissionObjectBase> permission_parent, struct db_thread_struct* thread_struct)
			: PermissionManagedObject(permission_parent, thread_struct->permission_object_id) {
	// this->cacheAllPermissions();
	this->id = thread_struct->id;
	this->deleted = thread_struct->deleted;
	this->number_of_posts = thread_struct->number_of_posts;
	// json thread_as_json;
	this->thread_as_json["id"] = this->id;
	this->thread_as_json["deleted"] = this->deleted;
	this->thread_as_json["number_of_posts"] = this->number_of_posts;
}

std::string Thread::dumpThread() const {
	return this->thread_as_json.dump();
}

void Thread::createPostFromStruct(struct db_post_struct* post_struct) {
	Post post(post_struct);
	if (post.getIdInThread() == 0) {
		this->thread_as_json["post_zero"] = post.asJson();
	}
	this->posts.emplace(post.getIdInThread(), post);
	// this->number_of_posts++;
	// this->thread_as_json["number_of_posts"] = this->number_of_posts;
	this->last_post_timestamp = post.getUploadTimestamp();
}

int Thread::createPostFromJson(json post_json) {
	post_json["id_in_thread"] = this->number_of_posts;
	// const std::string placeholder_key(KEY_LENGTH+1, 'T');
	// post_json["key"] = placeholder_key;
	Post post(post_json); // Key is deleted from post_json in its constructor
	if (this->number_of_posts == 0) {
		this->thread_as_json["post_zero"] = post.asJson();
	}
	this->posts.emplace(post.getIdInThread(), post);
	this->number_of_posts++; // number_of_posts gets updated in the database too, in db_store_post()
	this->thread_as_json["number_of_posts"] = this->number_of_posts;
	this->last_post_timestamp = post.getUploadTimestamp();
	return post.getIdInThread();
}

void Thread::deleteMessage(int message_id) {
	this->posts.at(message_id).markAsDeleted();
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
