#include <string>
#include <ctime>
#include <boost/json.hpp>
#include "FuzeDBI.hpp"
#include "field_lengths.h"

class Post {
public:
	// Post(struct db_post_struct* post_struct);
	Post(boost::json::object post_json, int author_client_id, FuzeDBI::Connection* fuze_dbi);
	std::string dumpPost() const;
	boost::json::object asJson() const { return this->post_as_json; };
	int getId() const { return this->id; };
	int getIdInThread() const { return this->id_in_thread; };
	// std::string getKey() const { return this->key; }
	std::chrono::time_point<std::chrono::system_clock> createdAt() const { return this->created_at; }
	void createFromJSON(boost::json::object post_json);
	void markAsDeleted();
	bool isDeleted() const { return this->deleted; }
private:
	int id;
	int thread_id;
	int id_in_thread;
	// char upload_timestamp[20];
	std::vector<std::string> files;
	short files_i;
	std::string name;
	std::string content;
	boost::json::object post_as_json;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	// std::string key;
	bool deleted;
};
