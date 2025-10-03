#include "board.hpp"
#include "db_interface.h"
#include <sstream>
#include <iostream>

Board::Board() {
	this->thread_limit=50; // MAX_THREADS_PER_BOARD
	this->post_limit = 100;
	this->cacheAllThreads();
}

int Board::createThread(json thread_json) {
	Thread thread(thread_json, true);
	this->threads.emplace(thread.getId(), thread);
	this->ordered_threads.insert(std::make_pair(thread.getLastPostTime(), thread.getId()));
	return thread.getId();
}

int Board::createPost(json post_json) {
	int thread_id = post_json["thread_id"].template get<int>();
	Thread* thread = &this->threads.at(thread_id);
	std::time_t old_post_time = thread->getLastPostTime();
	int new_post_id =  thread->createPostFromJson(post_json);
	std::time_t new_post_time = thread->getLastPostTime();
	if (old_post_time == new_post_time) {
		std::cerr << "POST TIMES ARE THE SAME" << std::endl;
	}
	this->ordered_threads.erase(std::make_pair(old_post_time, thread_id));
	this->ordered_threads.insert(std::make_pair(new_post_time, thread_id));
	return new_post_id;
}

void Board::deleteMessageFromThread(int message_id, int thread_id) {
	if (message_id != 0) {
		std::cout << "Deleting message " << message_id << " in thread " << thread_id << std::endl;
		this->threads.at(thread_id).deleteMessage(message_id);
	}
}

bool Board::keyMatchesMessageInThread(std::string key, int message_id, int thread_id) const {
	return this->threads.at(thread_id).keyMatchesMessage(key, message_id);
}

void Board::cacheAllThreads() {
	struct db_thread_array* thread_list = db_retrieve_threads();
	for (int i = 0; i < thread_list->used; i++) {
		json thread_json;
		thread_json["id"] = thread_list->array[i].id;
		// thread_json["number_of_posts"] = thread_list->array[i].number_of_posts;
		Thread thread(thread_json, false);
		this->threads.insert(std::make_pair(thread.getId(), thread));
	}
	freeThreadArray(thread_list);

	struct db_post_array* post_history = db_retrieve_history();
	for (int i = 0; i < post_history->used; i++) {
		this->threads.at(post_history->array[i].thread_id).createPostFromStruct(&post_history->array[i]);
	}
	for (std::map<int, Thread>::const_iterator it = this->threads.begin(); it != this->threads.end(); ++it) {
		this->ordered_threads.insert(std::make_pair(it->second.getLastPostTime(), it->first));
	}
	
	freePostArray(post_history);
}

std::string Board::dumpAllThreads() const {
	json multiple_thread_json;
	multiple_thread_json["type"] = "thread_catalog";
	multiple_thread_json["threads"] = json::array();
	for (auto it = this->ordered_threads.begin(); it != this->ordered_threads.end(); ++it) {
		multiple_thread_json["threads"].push_back(this->threads.at(it->second).asJson());
	}
	return multiple_thread_json.dump();
}

std::string Board::dumpPostsInThread(int thread_id, std::string key) const {
	return this->threads.at(thread_id).dumpPosts(key);
}

void Board::addListenerToThread(websocket_session* listener, int thread_id) {
	if (threadExists(thread_id)) {
		this->threads.at(thread_id).addListener(listener);
		std::cout << "Listener added to thread " << thread_id << std::endl;
	}
	else
		std::cout << "Warning: could not add listener to thread " << thread_id << " because the thread does not exist." << std::endl;
}

void Board::removeListenerFromThread(websocket_session* listener, int thread_id) {
	if (threadExists(thread_id)) {
		this->threads.at(thread_id).removeListener(listener);
		std::cout << "Listener removed from thread " << thread_id << std::endl;
	}
	else
		std::cout << "Warning: did not remove listener from thread " << thread_id << " because the thread does not exist." << std::endl;
}

std::string Board::dumpPost(int thread_id, int post_id, std::string key) const {
	return this->threads.at(thread_id).dumpPost(post_id, key);
}

/*
struct thread_order_comparator {
	bool operator() (std::pair<std::time_t, int> left, std::pair<std::time_t, int> right) const {
		if (left.first > right.first) {
			return true;
		}
		else if (left.first < right.first) {
			return false;
		}
		if (left.second > right.second) {
			return true;
		}
		else if (left.second < right.second) {
			return false;
		}
		else {
			std::cerr << "Error: cannot sort because threads have the same ID, This shouldn't happen" << std::endl;
			return false;
		}
	}
};
*/
