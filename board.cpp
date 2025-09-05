#include "board.hpp"
#include "db_interface.h"
#include <sstream>
#include <iostream>

Board::Board() {
	this->thread_limit=50; // MAX_THREADS_PER_BOARD
	this->post_limit = 100;
	this->cacheAllThreads();
}

void Board::createThread(json thread_json) {
	Thread thread(thread_json, true);
	this->threads.emplace(thread.getId(), thread);
}

int Board::createPost(json post_json) {
	return this->threads.at(post_json["thread_id"].template get<int>()).addPost(post_json, true);
}

void Board::cacheAllThreads() {
	struct db_thread_array* thread_list = db_retrieve_threads();
	for (int i = 0; i < thread_list->used; i++) {
		json thread_json;
		thread_json["id"] = thread_list->array[i].id;
		thread_json["number_of_posts"] = thread_list->array[i].number_of_posts;
		Thread thread(thread_json, false);
		this->threads.insert(std::make_pair(thread.getId(), thread));
	}
	freeThreadArray(thread_list);

	struct db_post_array* post_history = db_retrieve_history();
	for (int i = 0; i < post_history->used; i++) {
		json post_json;
		post_json["id"] = post_history->array[i].id;
		post_json["thread_id"] = post_history->array[i].thread_id;
		post_json["id_in_thread"] = post_history->array[i].id_in_thread;
		post_json["name"] = post_history->array[i].name;
		post_json["content"] = post_history->array[i].content;
		post_json["upload_timestamp"] = post_history->array[i].upload_timestamp;
		post_json["files"] = json::array();
		for (short file_i = 0; file_i < post_history->array[i].number_of_files; file_i++) {
			post_json["files"].push_back(post_history->array[i].files[file_i]);
		}
		if (post_history->array[i].id_in_thread == 0) {
			this->threads.at(post_history->array[i].thread_id).addInitialPost(post_json, false);
		}
		else {
			this->threads.at(post_history->array[i].thread_id).addPost(post_json, false);
		}
	}
	freePostArray(post_history);
}

std::string Board::dumpAllThreads() const {
	json multiple_thread_json;
	multiple_thread_json["type"] = "thread_catalog";
	multiple_thread_json["threads"] = json::array();
	for (auto it = this->threads.begin(); it != this->threads.end(); ++it) {
		multiple_thread_json["threads"].push_back(it->second.asJson());
	}
	return multiple_thread_json.dump();
}

std::string Board::dumpPostsInThread(int thread_id) const {
	return this->threads.at(thread_id).dumpPosts();
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

std::string Board::dumpPost(int thread_id, int post_id) const {
	return this->threads.at(thread_id).dumpPost(post_id);
}
