#include "user.hpp"
#include "db_interface.h"

User::User(struct db_account_struct* account_struct)
		: id(account_struct->id)
		, username(account_struct->username) {
}

// Register new user into the database
User::User(int id, std::string username)
		: id(0)
		, username(username) {
}
