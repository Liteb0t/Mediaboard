#include "DatabaseConnectionSQLite.hpp"
#include <iostream>
#include <fstream>

int callback(void* unused, int number_of_columns, char** columns, char** column_names) {
	int i;
	for(i = 0; i < number_of_columns; i++) {
		printf(" %s = %s |", column_names[i], columns[i] ? columns[i] : "NULL");
	}
	printf("\n");
	return 0;
}

DatabaseConnectionSQLite::DatabaseConnectionSQLite(boost::filesystem::path database_directory, std::string filename) {
	std::string database_filepath = std::format("{}/{}", database_directory.string(), filename);
	std::cout << "[DatabaseConnectionSQLite] Connecting to database at " << database_filepath << std::endl;
	int ec = sqlite3_open(database_filepath.c_str(), &db);
	if (ec) {
		std::cout << "[DatabaseConnectionSQLite] Can't open database: " << sqlite3_errmsg(db) << std::endl;
		sqlite3_close(db);
		return;
	}
	char* error_message;
	ec = sqlite3_exec(db, "SELECT version FROM _info", callback, 0, &error_message);
	if (ec != SQLITE_OK) { // We will assume this means the database is not populated
		fprintf(stderr, "[DatabaseConnectionSQLite] SQL error %d: %s\n", ec, error_message);
		sqlite3_free(error_message);
		this->init(database_directory);
		return;
	}
	// TODO check version if current and then make migrations if not
}

void DatabaseConnectionSQLite::init(boost::filesystem::path database_directory) {
	std::ifstream sqlite_template_file(std::format("{}/database_template_sqlite.sql", database_directory.string()));
	std::string line;
	char* error_message;
	while (std::getline(sqlite_template_file, line)) {
		std::cout << "[DatabaseConnectionSQLite] init line: " << line << std::endl;
		int ec = sqlite3_exec(db, line.c_str(), callback, 0, &error_message);
		if (ec != SQLITE_OK) {
			fprintf(stderr, "[DatabaseConnectionSQLite] SQL error %d: %s\n", ec, error_message);
			sqlite3_free(error_message);
			// return;
		}
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
