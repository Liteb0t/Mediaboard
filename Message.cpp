#include "Message.hpp"
#include <boost/json/serialize.hpp>
#include <chrono>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>

/*
// Cache post from database interface struct
Post::Post(struct db_post_struct* post_struct) {
	// std::cout << "createFromStruct ID: " << post_struct->id << std::endl;
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
	this->deleted = post_struct->deleted;
	// this->key = post_struct->key;

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
	// this->post_as_json["key"] = this->key;
}
*/
// Save post when JSON is received
Message::Message(boost::json::object post_json, int author_client_id, FuzeDBI::Connection* fuze_dbi) {
	if (!(post_json.contains("files") && post_json.contains("name") && post_json.contains("content"))) {
		throw std::runtime_error("Message JSON is missing one or more of the following entries: files, name, content");
	}
	// if ((message_content.length() == 0 && post_json["files"].size() == 0) || message_content.length() > static_cast<size_t>(MESSAGE_FIELDS::MAX_CONTENT))
	//	return api_response(http::status::bad_request, std::string("The post does not meet the constraints set by the server.\nThis could mean that the message content was empty and no files were uploaded, or the message content is too long."));
	this->post_as_json = post_json;
	this->post_as_json["type"] = "post";

	this->id_in_thread = post_json["id_in_thread"].as_int64();
	this->thread_id = post_json["thread_id"].as_int64();
	// User input checking is done on front-end, so it's not high priority to return an http error when username or content is empty/too long.
	this->name = post_json["name"].as_string();
	if (this->name == "") {
		this->name = "Anonymous"; // Blank username becoming Anonymous is intended behaviour
		this->post_as_json["name"] = this->name;
	}
	else if (this->name.length() > static_cast<size_t>(MESSAGE_FIELDS::MAX_NAME)) {
		this->name = "Bad Username"; 
		this->post_as_json["name"] = this->name;
	}
	this->content = post_json["content"].as_string();
	if (this->content.length() > static_cast<size_t>(MESSAGE_FIELDS::MAX_CONTENT)) {
		throw std::runtime_error(std::format("Message content length {} exceeds the limit of {}", this->content.length(), static_cast<size_t>(MESSAGE_FIELDS::MAX_CONTENT)));
	}
	this->created_at = std::chrono::system_clock::now();
	this->post_as_json["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count();

	// this->id = fuze_dbi->query<int>("SELECT message_id FROM _sequences");
	// fuze_dbi->query<void>("UPDATE _sequences SET message_id = $1", this->id + 1);
	// fuze_dbi->query<void>("INSERT INTO message(id, thread, id_in_thread, author_client_id, name, created_at, content) VALUES ($1, $2, $3, $4, $5, $6, $7)", this->id, this->thread_id, this->id_in_thread, author_client_id, this->name, std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count(), this->content);
	boost::json::array files_json = post_json["files"].as_array();
	if (this->content.length() == 0 && files_json.size() == 0) {
		throw std::runtime_error("Message Cannot be empty");
	}
	boost::json::array::const_iterator it = files_json.begin();
	this->files_i = 0;
	while (it != files_json.end() && files_i < 4) {
		const boost::json::string filename = it->as_string();
		if (filename.size() <= static_cast<size_t>(MESSAGE_FIELDS::MAX_FILE_NAME_WITH_UUID)) {
			this->files.push_back(filename.c_str());
			// fuze_dbi->query<void>("INSERT INTO message_file(message_id, file_name) VALUES ($1, $2)", this->id, filename.c_str());
		}
		else
			std::cerr << "File name too long to save to database. Length: " << filename.size() << std::endl;
		it++;
	}
	this->post_as_json["id"] = this->id;

	// New posts are not in a deleted state
	this->deleted = false;
	// Moderators will be able to view deleted messages
	// this->post_as_json["deleted"] = false;
}

std::string Message::dump() const {
	return boost::json::serialize(this->post_as_json);
}

void Message::markAsDeleted() {
	this->deleted = true;
	// db_mark_post_as_deleted(this->id);
}
