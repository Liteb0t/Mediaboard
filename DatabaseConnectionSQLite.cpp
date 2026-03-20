#include "DatabaseConnectionSQLite.hpp"
#include <iostream>

DatabaseConnectionSQLite::DatabaseConnectionSQLite(std::string file) {
	std::cout << "[DatabaseConnectionSQLite] Connecting to database...";
	int ec = sqlite3_open(file.c_str(), &db);
	if(ec) {
		std::cout << "[DatabaseConnectionSQLite] Can't open database: " << sqlite3_errmsg(db) << std::endl;
		sqlite3_close(db);
	}
}

DatabaseConnectionSQLite::~DatabaseConnectionSQLite() {
	std::cout << "[DatabaseConnectionSQLite] Closing connection..." << std::endl;
	sqlite3_close(db);
}

int DatabaseConnectionSQLite::storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) {
	std::cout << "[DatabaseConnectionSQLite] storePermissionCollection()... can't do that yet famalam" << std::endl;
	return -1;
}
