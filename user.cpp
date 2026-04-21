#include "user.hpp"
#include "db_interface.h"

User::User(struct db_account_struct* account_struct)
		: id(account_struct->id)
		, key(std::string(account_struct->key))
		, username(account_struct->username) {
}

// Register new user into the database
User::User(std::string username, std::string password) 
		: id(0)
		, key(std::string(""))
		, username(username) {
}
