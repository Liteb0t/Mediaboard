#include "post.hpp"
#include "db_interface.h"
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>

Post::Post(json post_json, bool save_to_database) {
	this->post_as_json = post_json;
	this->post_as_json["type"] = "post";
	this->name = post_json["name"].template get<std::string>();
	this->id_in_thread = post_json["id_in_thread"].template get<int>();
	this->thread_id = post_json["thread_id"].template get<int>();
	if (this->name == "") {
		this->name = "Anonymous";
		this->post_as_json["name"] = this->name;
	}
	std::cout << "name is \"" << this->name << "\"\n";
	this->content = post_json["content"].template get<std::string>();
	std::vector<std::string> file_vector = post_json["files"].template get<std::vector<std::string>>();
	std::vector<std::string>::iterator it = file_vector.begin();
	this->files_i = 0;
	while (it != file_vector.end()) {
		if ((*it).length() <= 255) {
			strcpy(this->files[files_i++], (*it).c_str());
		}
		it++;
	}
	// struct db_post_struct post_as_struct;
	// post_as_struct.name = this->name;
	// post_as_struct.content = this->content;
	if (save_to_database) {
		// ID and timestamp are not initially known
		std::time_t current_time = std::time(nullptr);
		// this->upload_timestamp[TIMESTAMP_LEN];
		std::strftime(this->upload_timestamp, TIMESTAMP_LEN, "%F %T", std::gmtime(&current_time));
		this->post_as_json["upload_timestamp"] = this->upload_timestamp;

		this->id = db_store_post(this->thread_id, this->id_in_thread, this->name.c_str(), this->upload_timestamp, this->content.c_str(), this->files, this->files_i);
		this->post_as_json["id"] = this->id;
	}
	else {
		this->id = post_json["id"].template get<int>();
	}
}

std::string Post::dumpPost() const {
	return this->post_as_json.dump();
}

