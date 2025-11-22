#include <string>
#include <ctime>
#include <nlohmann/json.hpp>
#include "field_lengths.h"

using json = nlohmann::json;

class Post {
public:
	Post(struct db_post_struct* post_struct);
	Post(json post_json);
	std::string dumpPost() const;
	json asJson() const { return this->post_as_json; };
	int getId() const { return this->id; };
	int getIdInThread() const { return this->id_in_thread; };
	std::string getKey() const { return this->key; }
	std::time_t getUploadTimestamp() const { return this->upload_timestamp; }
	void createFromJSON(json post_json);
	void markAsDeleted();
	bool isDeleted() const { return this->deleted; }
private:
	int id;
	int thread_id;
	int id_in_thread;
	// char upload_timestamp[20];
	std::time_t upload_timestamp;
	char files[4][POST_MAX_FILE_NAME_WITH_UUID+1];
	short files_i;
	std::string name;
	std::string content;
	json post_as_json;
	std::string key;
	bool deleted;
};
