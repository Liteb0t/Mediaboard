#include "DatabaseConnectionPostgreSQL.hpp"
#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include <iostream>

DatabaseConnectionPostgreSQL::DatabaseConnectionPostgreSQL(std::string connection_target) {
	db_connect(connection_target.c_str());
}

DatabaseConnectionPostgreSQL::~DatabaseConnectionPostgreSQL() {
	std::cout << "[DatabaseConnectionPostgreSQL] Closing connection..." << std::endl;
	db_disconnect();
}

int DatabaseConnectionPostgreSQL::storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) {
	return db_store_permission_collection(permission_object_id,
								   user_or_group == USER_OR_GROUP::USER ? user_or_group_id : -1,
								   user_or_group == USER_OR_GROUP::GROUP ? user_or_group_id : -1);
}
