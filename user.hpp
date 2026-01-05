#include <string>
#include "db_interface.h"
// #include <nlohmann/json.hpp>

// using json = nlohmann::json;

enum BUILTIN_USERS {
	PUBLIC = 0,
	ADMINISTRATOR = 1
};

class User {
public:
	User(struct db_account_struct* account_struct);
	// User(int id, std::string name);
	User(std::string username, std::string password);
	// User(json user_json);
	bool passwordMatches(std::string password) const {
		return db_account_matches_password(this->id, password.c_str());
	}
	bool keyMatches(std::string key) const {
		return key == this->key;
	}
	int getId() const { return this->id; }
	std::string getUsername() const { return this->username; }
	std::string getKey() const { return this->key; }
private:
	int id;
	std::string username;
	std::string key;
};
