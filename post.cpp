#include "post.hpp"
#include "db_interface.h"
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>

// Cache post from database interface struct
Post::Post(struct db_post_struct* post_struct) {
	std::cout << "createFromStruct ID: " << post_struct->id << std::endl;
	this->id = post_struct->id;
	this->thread_id = post_struct->thread_id;
	this->id_in_thread = post_struct->id_in_thread;
	this->name = post_struct->name;
	this->content = post_struct->content;
	this->upload_timestamp = post_struct->upload_timestamp;
	this->files_i = post_struct->number_of_files;
	for (short file_i = 0; file_i < this->files_i; file_i++) {
		// this->files[file_i] = post_struct->files[file_i];
		strcpy(this->files[file_i], post_struct->files[file_i]);
	}

	this->post_as_json["id"] = this->id;
	this->post_as_json["thread_id"] = this->thread_id;
	this->post_as_json["id_in_thread"] = this->id_in_thread;
	this->post_as_json["name"] = this->name;
	this->post_as_json["content"] = this->content;
	this->post_as_json["upload_timestamp"] = this->upload_timestamp;
	this->post_as_json["files"] = json::array();
	for (short file_i = 0; file_i < this->files_i; file_i++) {
		this->post_as_json["files"].push_back(this->files[file_i]);
	}
}

// Save post when JSON is received
Post::Post(json post_json) {
	this->post_as_json = post_json;
	this->post_as_json["type"] = "post";

	this->id_in_thread = post_json["id_in_thread"].template get<int>();
	this->thread_id = post_json["thread_id"].template get<int>();
	this->name = post_json["name"].template get<std::string>();
	if (this->name == "") {
		this->name = "Anonymous";
		this->post_as_json["name"] = this->name;
	}
	std::cout << "name is \"" << this->name << "\"\n";
	this->content = post_json["content"].template get<std::string>();

	// vector should be replaced with an array. that requires editing the json template
	std::vector<std::string> file_vector = post_json["files"].template get<std::vector<std::string>>();
	std::vector<std::string>::iterator it = file_vector.begin();
	this->files_i = 0;
	while (it != file_vector.end()) {
		if ((*it).length() <= 255) {
			strcpy(this->files[files_i++], (*it).c_str());
		}
		it++;
	}
	// ID and timestamp are not initially known
	this->upload_timestamp = std::time(nullptr);
	// std::strftime(this->upload_timestamp, TIMESTAMP_LEN, "%F %T", std::gmtime(&current_time));
	this->post_as_json["upload_timestamp"] = this->upload_timestamp;

	this->id = db_store_post(this->thread_id, this->id_in_thread, this->name.c_str(), this->upload_timestamp, this->content.c_str(), this->files, this->files_i);
	this->post_as_json["id"] = this->id;
}

std::string Post::dumpPost() const {
	return this->post_as_json.dump();
}

