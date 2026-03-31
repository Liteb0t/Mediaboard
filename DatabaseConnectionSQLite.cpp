#include "DatabaseConnectionSQLite.hpp"
#include "db_interface.h"
#include <format>
#include <iostream>
#include <fstream>
#include <sqlite3.h>
#include <stdexcept>
#include <sstream>
#include <cstring>

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

DatabaseConnectionSQLite::~DatabaseConnectionSQLite() {
	std::cout << "[DatabaseConnectionSQLite] Closing connection..." << std::endl;
	sqlite3_close(db);
}

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
	ec = sqlite3_prepare_v2(this->db, std::format("SELECT id, account_id, permission_group_id FROM permission_collection WHERE permission_object_id = {}", permission_object_id).c_str(), -1, &this->stmt, NULL);
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
			if (sqlite3_column_type(stmt, 1) != SQLITE_NULL)
				permission_collection.account_id = sqlite3_column_int(stmt, 1);
			else
				permission_collection.account_id = -1;
			if (sqlite3_column_type(stmt, 2) != SQLITE_NULL)
				permission_collection.group_id = sqlite3_column_int(stmt, 2);
			else
				permission_collection.group_id = -1;
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
			std::cerr << "[DatabaseConnectionSQLite] getValueFromPermissionSettingCursor() Error: " << sqlite3_errmsg(this->db);
			break;
	}
	return &permission_setting;
}
void DatabaseConnectionSQLite::closePermissionSettingCursor() {
	sqlite3_finalize(this->stmt2);
}
/*
void DatabaseConnectionSQLite::TestIteratorSQLite::printClassType() const {
	std::cout << "[TestIteratorSQLite] type is " << this->db->getClassType() << std::endl;
}*/
void DatabaseConnectionSQLite::declareAccountCursor() {
	int ec;
	ec = sqlite3_prepare_v2(this->db, "SELECT id, username, key FROM account" , -1, &this->stmt, NULL);
	if (ec != SQLITE_OK) {
		std::cerr << "[DatabaseConnectionSQLite] Could not declare cursor: " << sqlite3_errmsg(this->db);
		return /* failure */;
	}
}
db_account_struct* DatabaseConnectionSQLite::getValueFromAccountCursor() {
	int ec;
	static db_account_struct account;
	switch (sqlite3_step(this->stmt)) {
		case SQLITE_ROW:
			account.has_value = true;
			account.id = sqlite3_column_int(this->stmt, 0);
			if (sqlite3_column_type(this->stmt, 1) != SQLITE_NULL) {
				const unsigned char* text = sqlite3_column_text(this->stmt, 1);
				strcpy(account.username, reinterpret_cast<const char*>(text));
			}
			else
				account.username[0] = '\0';
			if (sqlite3_column_type(this->stmt, 2) != SQLITE_NULL) {
				const unsigned char* text = sqlite3_column_text(this->stmt, 2);
				strcpy(account.key, reinterpret_cast<const char*>(text));
			}
			else
				account.key[0] = '\0';
			break;
		case SQLITE_DONE:
			account.has_value = false;
			break;
		default:
			account.has_value = false;
			std::cerr << "[DatabaseConnectionSQLite] getValueFromPermissionSettingCursor() Error: " << sqlite3_errmsg(this->db);
			break;
	}
	return &account;
}
void DatabaseConnectionSQLite::closeAccountCursor() {
	sqlite3_finalize(this->stmt);
}
void DatabaseConnectionSQLite::declareGroupCursor() {
	int ec;
	ec = sqlite3_prepare_v2(this->db, "SELECT id, name FROM permission_group" , -1, &this->stmt, NULL);
	if (ec != SQLITE_OK) {
		std::cerr << "[DatabaseConnectionSQLite] Could not declare cursor: " << sqlite3_errmsg(this->db);
		return /* failure */;
	}
}
db_group_struct* DatabaseConnectionSQLite::getValueFromGroupCursor() {
	int ec;
	static db_group_struct group;
	switch (sqlite3_step(this->stmt)) {
		case SQLITE_ROW:
			group.has_value = true;
			group.id = sqlite3_column_int(this->stmt, 0);
			if (sqlite3_column_type(this->stmt, 1) != SQLITE_NULL) {
				const unsigned char* text = sqlite3_column_text(this->stmt, 1);
				strcpy(group.name, reinterpret_cast<const char*>(text));
			}
			else
				group.name[0] = '\0';
			break;
		case SQLITE_DONE:
			group.has_value = false;
			break;
		default:
			group.has_value = false;
			std::cerr << "[DatabaseConnectionSQLite] getValueFromPermissionSettingCursor() Error: " << sqlite3_errmsg(this->db);
			break;
	}
	return &group;
}
void DatabaseConnectionSQLite::closeGroupCursor() {
	sqlite3_finalize(this->stmt);
}

void DatabaseConnectionSQLite::declareGroupHeirarchyCursor() {
	int ec;
	ec = sqlite3_prepare_v2(this->db, "SELECT rank, permission_group FROM permission_group_heirarchy" , -1, &this->stmt, NULL);
	if (ec != SQLITE_OK) {
		std::cerr << "[DatabaseConnectionSQLite] Could not declare cursor: " << sqlite3_errmsg(this->db);
		return /* failure */;
	}
}
db_group_heirarchy_struct* DatabaseConnectionSQLite::getValueFromGroupHeirarchyCursor() {
	int ec;
	static db_group_heirarchy_struct group_heirarchy;
	switch (sqlite3_step(this->stmt)) {
		case SQLITE_ROW:
			group_heirarchy.has_value = true;
			group_heirarchy.rank = sqlite3_column_int(stmt, 0);
			group_heirarchy.group_id = sqlite3_column_int(stmt, 1);
			break;
		case SQLITE_DONE:
			group_heirarchy.has_value = false;
			break;
		default:
			group_heirarchy.has_value = false;
			std::cerr << "[DatabaseConnectionSQLite] getValueFromGroupHeirarchyCursor() Error: " << sqlite3_errmsg(this->db);
			break;
	}
	return &group_heirarchy;
}
void DatabaseConnectionSQLite::closeGroupHeirarchyCursor() {
	sqlite3_finalize(this->stmt);
}

void DatabaseConnectionSQLite::declareGroupMemberCursor() {
	int ec;
	ec = sqlite3_prepare_v2(this->db, "SELECT group_id, account_id FROM permission_group_account" , -1, &this->stmt, NULL);
	if (ec != SQLITE_OK) {
		std::cerr << "[DatabaseConnectionSQLite] Could not declare cursor: " << sqlite3_errmsg(this->db);
		return /* failure */;
	}
}
db_group_member_struct* DatabaseConnectionSQLite::getValueFromGroupMemberCursor() {
	int ec;
	static db_group_member_struct group_member;
	switch (sqlite3_step(this->stmt)) {
		case SQLITE_ROW:
			group_member.has_value = true;
			group_member.group_id = sqlite3_column_int(stmt, 0);
			group_member.account_id = sqlite3_column_int(stmt, 1);
			break;
		case SQLITE_DONE:
			group_member.has_value = false;
			break;
		default:
			group_member.has_value = false;
			std::cerr << "[DatabaseConnectionSQLite] getValueFromGroupHeirarchyCursor() Error: " << sqlite3_errmsg(this->db);
			break;
	}
	return &group_member;
}
void DatabaseConnectionSQLite::closeGroupMemberCursor() {
	sqlite3_finalize(this->stmt);
}
