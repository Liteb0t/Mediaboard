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
		this->addPost(thread_json["post_zero"], true);
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

void Thread::addInitialPost(json post_json, bool save_to_database) {
	post_json["id_in_thread"] = 0;
	Post post_zero(post_json, save_to_database);
	// this->posts[0] = post_zero;
	this->posts.emplace(post_zero.getId(), post_zero);
	this->thread_as_json["post_zero"] = post_zero.asJson();
	this->number_of_posts = 1;
	this->thread_as_json["number_of_posts"] = 1;
}

int Thread::addPost(json post_json, int id_in_thread, bool save_to_database) {
	post_json["id_in_thread"] = id_in_thread;
	Post post(post_json, save_to_database);
	if (id_in_thread == 0) {
		this->thread_as_json["post_zero"] = post.asJson();
	}
	this->posts.emplace(post.getId(), post);
	this->number_of_posts++;
	// this->last_upload_timestamp = ???
	this->thread_as_json["number_of_posts"] = this->number_of_posts;
	return post.getId();
}

int Thread::addPost(json post_json, bool save_to_database) {
	return this->addPost(post_json, this->number_of_posts, save_to_database);
	// Post post(post_json, save_to_database);
	// this->posts.emplace(post.getId(), post);
}

std::string Thread::dumpPosts() const {
	json multiple_post_json;
	multiple_post_json["type"] = "post_history";
	multiple_post_json["posts"] = json::array();
	for (auto it = this->posts.begin(); it != this->posts.end(); ++it) {
		std::cout << "Dumping post " << it->second.getId() << std::endl;
		multiple_post_json["posts"].push_back(it->second.asJson());
	}
	return multiple_post_json.dump();
}

std::string Thread::dumpPost(int post_id) const {
	return this->posts.at(post_id).dumpPost();
}

void Thread::addListener(websocket_session* listener) {
	listeners.insert(listener);
}

void Thread::removeListener(websocket_session* listener) {
	listeners.erase(listener);
}
