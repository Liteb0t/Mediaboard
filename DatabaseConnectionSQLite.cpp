#include "DatabaseConnectionSQLite.hpp"
#include "db_interface.h"
#include <format>
#include <iostream>
#include <fstream>
#include <sqlite3.h>
#include <stdexcept>
#include <string_view>
#include <sstream>

int callback(void* unused, int number_of_columns, char** columns, char** column_names) {
	int i;
	for(i = 0; i < number_of_columns; i++) {
		printf(" %s = %s |", column_names[i], columns[i] ? columns[i] : "NULL");
	}
	printf("\n");
	return 0;
}

void DatabaseConnectionSQLite::execWriteOnlyStatement(std::string& statement) {
	this->execWriteOnlyStatement(statement.c_str());
}
void DatabaseConnectionSQLite::execWriteOnlyStatement(const char* statement) {
	std::cout << "[DatabaseConnectionSQLite] " << statement << std::endl;
	int ec; char* error_message;
	ec = sqlite3_exec(this->db, statement, callback, 0, &error_message);
	if (ec != SQLITE_OK) {
		std::string error_str = error_message;
		sqlite3_free(error_message);
		throw std::runtime_error(error_str);
		// fprintf(stderr, "[DatabaseConnectionSQLite] SQL error %d: %s\n", ec, error_message);
	}
}

void DatabaseConnectionSQLite::execMultipleWriteOnlyStatements(std::istream& stream) {
	std::string line;
	while (std::getline(stream, line, ';')) {
		this->execWriteOnlyStatement(line);
	}
}

void DatabaseConnectionSQLite::writeDatabaseVersion(const std::string& program_version_string) {
	this->execWriteOnlyStatement("CREATE TABLE IF NOT EXISTS _info(version TEXT NOT NULL)");
	this->execWriteOnlyStatement("DELETE FROM _info");
	this->execWriteOnlyStatement(std::format("INSERT INTO _info VALUES('{}')", program_version_string).c_str());
}

DatabaseConnectionSQLite::DatabaseConnectionSQLite(const boost::filesystem::path& database_directory, const std::string& filename, const std::string& program_version_string) {
	std::string database_filepath = std::format("{}/{}", database_directory.string(), filename);
	std::cout << "[DatabaseConnectionSQLite] Connecting to database at " << database_filepath << std::endl;
	int ec = sqlite3_open(database_filepath.c_str(), &db);
	if (ec) {
		std::cout << "[DatabaseConnectionSQLite] Can't open database: " << sqlite3_errmsg(db) << std::endl;
		sqlite3_close(db);
		return;
	}
	// char* error_message;
	sqlite3_stmt* stmt;
	ec = sqlite3_prepare_v2(this->db, "SELECT version FROM _info", -1, &stmt, NULL);
	if (ec != SQLITE_OK) { // We will assume this means the database is not populated
		// fprintf(stderr, "[DatabaseConnectionSQLite] SQL error %d: %s\n", ec, error_message);
		// sqlite3_free(error_message);
		// this->firstTimeSetup(database_directory, program_version_string);
		try {
			std::ifstream sqlite_template_file(std::format("{}/database_template_sqlite.sql", database_directory.string()));
			this->execMultipleWriteOnlyStatements(sqlite_template_file);
			sqlite_template_file.close();
			this->writeDatabaseVersion(program_version_string);
		}
		catch (std::exception& exception) {
			std::cout << "[DatabaseConnectionSQLite] Exception occured in constructor: " << exception.what() << std::endl;
		}
	}
	else {
		int number_of_columns = sqlite3_column_count(stmt);
		// std::cout << "[DatabaseConnectionSQLite] number_of_columns: " << number_of_columns << std::endl;
		if (sqlite3_step(stmt) == SQLITE_ROW || sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
			const unsigned char* text = sqlite3_column_text(stmt, 0); //get the value from at that column as text
			// printf("= %s \n", text);
			std::string database_version_string = std::string(reinterpret_cast<const char*>(text));
			std::cout << database_version_string << " (database) : " << program_version_string << " (server)" << std::endl;
			if (database_version_string < program_version_string) {
				std::cout << "[DatabaseConnectionSQLite] Mediaboard version " << program_version_string << " is newer than the database version " << database_version_string << ". Migrations will be made if necessary." << std::endl;
				std::stringstream migrations;
				if (this->writeMigrations(migrations, database_version_string)) {
					try {
						this->execMultipleWriteOnlyStatements(migrations);
					}
					catch (std::exception& exception) {
						std::cout << "[DatabaseConnectionSQLite] Exception occured in constructor: " << exception.what() << std::endl;
					}
					std::cout << "[DatabaseConnectionSQLite] Finished doing migrations." << std::endl;
				}
				else {
					std::cout << "[DatabaseConnectionSQLite] No migrations necessary." << std::endl;
				}
				migrations.clear();
				try {
					this->writeDatabaseVersion(program_version_string);
				}
				catch (std::exception& exception) {
					std::cout << "[DatabaseConnectionSQLite] Exception occured in constructor: " << exception.what() << std::endl;
				}
			}
			else if (database_version_string > program_version_string)
				std::cout << "[DatabaseConnectionSQLite] WARNING! database version is found to be newer than this server. Issues may occur. Consider updating to a newer version of Fuze Mediaboard." << std::endl;
			else
				std::cout << "[DatabaseConnectionSQLite] Database is up-to-date." << std::endl;
		}
		else {
			std::cout << "[DatabaseConnectionSQLite] No version string found. Database is bugged out." << std::endl;
		}
		sqlite3_finalize(stmt);
	}
}

bool DatabaseConnectionSQLite::writeMigrations(std::ostream& stream, const std::string& database_version_string) {
	if (database_version_string <= "0.0.5")	goto v0_0_5;
	// If code reaches here, no migrations need to be made
	return false;
v0_0_5:
	stream << "UPDATE permission_collection SET account_id = NULL WHERE account_id = -1;";
	stream << "UPDATE permission_collection SET permission_group_id = NULL WHERE permission_group_id = -1;";
	std::cout << "Finished writing migrations" << std::endl;
	return true; // Migrations were made
}

void DatabaseConnectionSQLite::firstTimeSetup(const boost::filesystem::path& database_directory, const std::string& program_version_string) {
	int ec; char* error_message;
	std::ifstream sqlite_template_file(std::format("{}/database_template_sqlite.sql", database_directory.string()));
	std::string line;
	try {
		while (std::getline(sqlite_template_file, line)) {
			std::cout << "[DatabaseConnectionSQLite] " << line << std::endl;
			this->execWriteOnlyStatement(line);
		}
		this->execWriteOnlyStatement("CREATE TABLE _info(version TEXT NOT NULL)");
		this->execWriteOnlyStatement("INSERT INTO _info VALUES('0.1')");
	}
	catch (std::exception& exception) {
		std::cout << "[DatabaseConnectionSQLite] Exception in SQLite init: " << exception.what() << std::endl;
	}
}

DatabaseConnectionSQLite::~DatabaseConnectionSQLite() {
	std::cout << "[DatabaseConnectionSQLite] Closing connection..." << std::endl;
	sqlite3_close(db);
}

int DatabaseConnectionSQLite::storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) {
	int new_permission_collection_id;
	this->execWriteOnlyStatement(std::format("EXEC SQL INSERT INTO permission_collection(permission_object_id, account_id, permission_group_id) VALUES ({}, {}, {})", permission_object_id, user_or_group == USER_OR_GROUP::USER ? std::to_string(user_or_group_id) : std::string("NULL"), user_or_group == USER_OR_GROUP::GROUP ? std::to_string(user_or_group_id) : std::string("NULL")).c_str())
	;
	int ec = sqlite3_prepare_v2(this->db, "SELECT last_insert_rowid()", -1, &this->stmt, NULL);
	if (ec == SQLITE_OK && sqlite3_step(this->stmt) == SQLITE_ROW) {
		new_permission_collection_id = sqlite3_column_int(stmt, 0);
	}
	else {
		throw std::runtime_error("[DatabaseConnectionSQLite] Couldn't get new ID of store permission_collection");
	}
	sqlite3_finalize(this->stmt);
	return new_permission_collection_id;
}

void DatabaseConnectionSQLite::declarePermissionCollectionCursor(int permission_object_id) {
	int ec;
	ec = sqlite3_prepare_v2(this->db, std::format("SELECT id, permission_group_id, account_id FROM permission_collection WHERE permission_object_id = {}", permission_object_id).c_str(), -1, &this->stmt, NULL);
	if (ec != SQLITE_OK) {
		std::cerr << "[DatabaseConnectionSQLite] Could not declare cursor: " << sqlite3_errmsg(this->db);
		return /* failure */;
	}
}
db_permission_collection_struct* DatabaseConnectionSQLite::getValueFromPermissionCollectionCursor() {
	int ec;
	static db_permission_collection_struct permission_collection;
	switch (sqlite3_step(this->stmt)) {
		case SQLITE_ROW:
			permission_collection.has_value = true;
			permission_collection.id = sqlite3_column_int(stmt, 0);
			permission_collection.group_id = sqlite3_column_int(stmt, 1);
			permission_collection.account_id = sqlite3_column_int(stmt, 2);
			break;
		case SQLITE_DONE:
			permission_collection.has_value = false;
			break;
		default:
			permission_collection.has_value = false;
			std::cerr << "[DatabaseConnectionSQLite] getValueFromPermissionCollectionCursor() Error: " <<sqlite3_errmsg(this->db);
			break;
	}
	return &permission_collection;
}
void DatabaseConnectionSQLite::closePermissionCollectionCursor() {
	sqlite3_finalize(this->stmt);
}

void DatabaseConnectionSQLite::declarePermissionSettingCursor(int permission_collection_id) {
	int ec;
	ec = sqlite3_prepare_v2(this->db, std::format("SELECT id, permission_number, setting FROM permission_setting WHERE permission_collection_id = {}", permission_collection_id).c_str(), -1, &this->stmt2, NULL);
	if (ec != SQLITE_OK) {
		std::cerr << "[DatabaseConnectionSQLite] Could not declare cursor: " << sqlite3_errmsg(this->db);
		return /* failure */;
	}
}
db_permission_setting_struct* DatabaseConnectionSQLite::getValueFromPermissionSettingCursor() {
	int ec;
	static db_permission_setting_struct permission_setting;
	switch (sqlite3_step(this->stmt2)) {
		case SQLITE_ROW:
			permission_setting.has_value = true;
			permission_setting.id = sqlite3_column_int(stmt2, 0);
			permission_setting.permission_number = sqlite3_column_int(stmt2, 1);
			permission_setting.setting = sqlite3_column_int(stmt2, 2);
			break;
		case SQLITE_DONE:
			permission_setting.has_value = false;
			break;
		default:
			permission_setting.has_value = false;
			std::cerr << "[DatabaseConnectionSQLite] getValueFromPermissionSettingCursor() Error: " <<sqlite3_errmsg(this->db);
			break;
	}
	return &permission_setting;
}
void DatabaseConnectionSQLite::closePermissionSettingCursor() {
	sqlite3_finalize(this->stmt2);
}
