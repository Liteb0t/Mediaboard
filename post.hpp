#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Post {
public:
	Post(json post_json, bool save_to_database);
	std::string dumpPost() const;
	json asJson() const { return this->post_as_json; };
	int getId() const { return this->id; };
private:
	int id;
	int thread_id;
	int id_in_thread;
	char upload_timestamp[20];
	char files[4][256]; // Max files is 4, maximum URL length is 255
	short files_i;
	std::string name;
	std::string content;
	json post_as_json;
};
