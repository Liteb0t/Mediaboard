#include "thread.hpp"
#include "db_interface.h"
#include <string>
#include <iostream>
#include <cstring>

Thread::Thread(json thread_json, bool save_to_database) {
	this->thread_as_json = thread_json;
	this->number_of_posts = 0;
	// struct db_thread_struct thread_as_struct;
	// thread_as_struct.name = this->name;
	// thread_as_struct.content = this->content;
	if (save_to_database) {
		// ID and timestamp are not initially known
		this->id = db_store_thread(1/*subject.c_str()*/);
		std::cout << "this->id: " << this->id << std::endl;
		this->thread_as_json["id"] = this->id;
		thread_json["post_zero"]["thread_id"] = this->id;
		const std::string placeholder_key(KEY_LENGTH+1, 'T');
		thread_json["post_zero"]["key"] = placeholder_key;
		this->createPostFromJson(thread_json["post_zero"]);
		thread_json["post_zero"].erase("key");
		// Post post_zero(thread_json["post_zero"], true);
		// this->thread_as_json["post_zero"] = post_zero.asJson();
		// this->posts[0] = post_zero;
		// this->number_of_posts = 1;
		// this->thread_as_json["number_of_posts"] = 1;
	}
	else {
		this->id = thread_json["id"].template get<int>();
		// this->number_of_posts = this->thread_as_json["number_of_posts"].template get<int>();
	}
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
	this->number_of_posts++;
	this->last_post_timestamp = post.getUploadTimestamp();
	this->thread_as_json["number_of_posts"] = this->number_of_posts;
}

int Thread::createPostFromJson(json post_json) {
	post_json["id_in_thread"] = this->number_of_posts;
	Post post(post_json);
	if (this->number_of_posts == 0) {
		this->thread_as_json["post_zero"] = post.asJson();
	}
	this->posts.emplace(post.getIdInThread(), post);
	this->number_of_posts++;
	this->last_post_timestamp = post.getUploadTimestamp();
	this->thread_as_json["number_of_posts"] = this->number_of_posts;
	return post.getIdInThread();
}

void Thread::deleteMessage(int message_id) {
	this->posts.at(message_id).markAsDeleted();
	std::cout << "Erased message " << message_id << " from thread " << this->id << std::endl;
}

bool Thread::keyMatchesMessage(std::string key, int message_id) const {
	std::cout << "[Thread] key :: message_key\n" << key << " :: " << this->posts.at(message_id).getKey() << std::endl;
	return this->posts.at(message_id).getKey() == key;
}

std::string Thread::dumpPosts(std::string key) const {
	json multiple_post_json;
	multiple_post_json["type"] = "post_history";
	multiple_post_json["posts"] = json::array();
	for (auto it = this->posts.begin(); it != this->posts.end(); ++it) {
		if (!it->second.isDeleted()) {
			std::cout << "Dumping post " << this->id << "/" << it->second.getIdInThread() << std::endl;
			json post_json = it->second.asJson();
			post_json["is_author"] = keyMatchesMessage(key, it->first);
			multiple_post_json["posts"].push_back(post_json);
		}
	}
	return multiple_post_json.dump();
}

std::string Thread::dumpPost(int message_id, std::string key) const {
	json post_json = this->posts.at(message_id).asJson();
	post_json["is_author"] = keyMatchesMessage(key, message_id);
	return post_json.dump();
}

void Thread::addListener(websocket_session* listener) {
	listeners.insert(listener);
}

void Thread::removeListener(websocket_session* listener) {
	listeners.erase(listener);
}
