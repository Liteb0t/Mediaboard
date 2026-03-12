#include "user.hpp"
#include "db_interface.h"

User::User(struct db_account_struct* account_struct)
		: id(account_struct->id)
		, key(std::string(account_struct->key))
		, username(account_struct->username) {
}

// Register new user into the database
User::User(std::string username, std::string password) 
		: id(db_store_account(username.c_str(), password.c_str()))
		, key(std::string(db_fetch_key(username.c_str(), password.c_str()))) // TODO remove inefficiency caused by database searching for username
		, username(username) {
}
