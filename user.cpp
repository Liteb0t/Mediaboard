#include "user.hpp"
#include <iostream>

User::User(struct db_account_struct* account_struct) {
	this->id = account_struct->id;
	this->username = account_struct->username;
	this->key = std::string(account_struct->key);
	if (this->key.length() != KEY_LENGTH)
		std::cerr << "User key length " << this->key.length() << " does not match macro KEY_LENGTH" << std::endl;
}

// User::User(int id, std::string username) 
// 		: id(id), username(username) {}

// Register new user into the database
User::User(std::string username, std::string password) 
		: username(username) {
	this->id = db_store_account(username.c_str(), password.c_str());
	char key[8+1];
	db_fetch_key(key, username.c_str(), password.c_str()); // TODO remove inefficiency caused by database searching for username
	this->key = std::string(key);
}
/*
User::User(json user_json) {
	this->username = user_json["username"].template get<std::string>();
	std::string password = user_json["password"].template get<std::string>();
	this->id = db_store_account(this->username.c_str(), password.c_str());
}*/
