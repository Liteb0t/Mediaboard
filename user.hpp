#include <string>
#include "db_interface.h"

class User {
public:
	static const int PUBLIC = 0;
	User(struct db_account_struct* account_struct);
	User(int id, std::string username);
	// bool passwordMatches(std::string password) const {
	// 	return db_account_matches_password(this->id, password.c_str());
	// }
	// bool keyMatches(std::string key) const {
	// 	return key == this->key;
	// }
	const int& getId() const { return this->id; }
	// const std::string& getKey() const { return this->key; }
	const std::string& getUsername() const { return this->username; }
private:
	const int id;
	// const std::string key;
	const std::string username;
};
