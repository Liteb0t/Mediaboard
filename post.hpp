#include <string>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Post {
public:
	Post(struct db_post_struct* post_struct);
	Post(json post_json);
	std::string dumpPost() const;
	json asJson() const { return this->post_as_json; };
	int getId() const { return this->id; };
	int getIdInThread() const { return this->id_in_thread; };
	std::time_t getUploadTimestamp() const { return this->upload_timestamp; }
	void createFromJSON(json post_json);
private:
	int id;
	int thread_id;
	int id_in_thread;
	// char upload_timestamp[20];
	std::time_t upload_timestamp;
	char files[4][256]; // Max files is 4, maximum URL length is 255
	short files_i;
	std::string name;
	std::string content;
	json post_as_json;
};
