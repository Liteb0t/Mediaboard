#include "DatabaseConnectionPostgreSQL.hpp"
#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include <format>
#include <iostream>
#include <libpq-fe.h>
#include <sqlite3.h>
#include <sstream>

DatabaseConnectionPostgreSQL::DatabaseConnectionPostgreSQL(const std::string& postgresql_uri, const std::string& program_version_string) {
	this->db = PQconnectdb(postgresql_uri.c_str());
	this->migrateIfVersionIsNewer(program_version_string);
}
DatabaseConnectionPostgreSQL::DatabaseConnectionPostgreSQL(const std::string& postgresql_user, const std::string& postgresql_host, const unsigned short postgresql_port, const std::string& postgresql_database_name, const std::string& program_version_string) {
	std::string libpq_connection_string = std::format("user={} host={} port={} dbname={}", postgresql_user, postgresql_host, postgresql_port, postgresql_database_name);
	std::cout << "[DatabaseConnectionPostgreSQL] libpq connection string: " << libpq_connection_string << std::endl;
	this->db = PQconnectdb(libpq_connection_string.c_str());
	switch (PQstatus(this->db)) {
		case CONNECTION_BAD:
			std::cerr << "[DatabaseConnectionPostgreSQL] Could not connect via libpq: " << PQerrorMessage(this->db) << std::endl;
			break;
		case CONNECTION_OK:
			std::cout << "[DatabaseConnectionPostgreSQL] Connected via libpq successfully" << std::endl;
			break;
		default:
			std::cerr << "[DatabaseConnectionPostgreSQL] Unknown libpq connection status" << std::endl;
			break;
	}

	db_connect(std::format("{}@{}:{}", postgresql_database_name, postgresql_host, postgresql_port).c_str(), postgresql_user.c_str());
	this->migrateIfVersionIsNewer(program_version_string);
}

DatabaseConnectionPostgreSQL::~DatabaseConnectionPostgreSQL() {
	std::cout << "[DatabaseConnectionPostgreSQL] Closing connection..." << std::endl;
	db_disconnect();
}

void DatabaseConnectionPostgreSQL::execWriteOnlyStatement(const std::string& statement) {
	this->execWriteOnlyStatement(statement.c_str());
}
void DatabaseConnectionPostgreSQL::execWriteOnlyStatement(const char* statement) {
	std::cout << "[DatabaseConnectionPostgreSQL] " << statement << std::endl;
	int ec; char* error_message;
	PGresult* result =  PQexec(this->db, statement);
	ExecStatusType status = PQresultStatus(result);
	switch (status) {
		case PGRES_EMPTY_QUERY:
			std::cout << "[DatabaseConnectionPostgreSQL] Warning: SQL statement was empty." << std::endl;
		case PGRES_COMMAND_OK:
			break;
		case PGRES_FATAL_ERROR:
			throw std::runtime_error(std::format("Fatal error: {}", PQerrorMessage(this->db)));
			// fprintf(stderr, "[DatabaseConnectionSQLite] SQL error %d: %s\n", ec, error_message);
			break;
		default:
			throw std::runtime_error(std::format("Unknown PWresStatus: {}", PQresStatus(status)));
			break;
	}
}

void DatabaseConnectionPostgreSQL::execMultipleWriteOnlyStatements(std::istream& stream) {
	std::string line;
	while (std::getline(stream, line, ';')) {
		this->execWriteOnlyStatement(line);
	}
}

void DatabaseConnectionPostgreSQL::writeDatabaseVersion(const std::string& program_version_string) {
	// this->execWriteOnlyStatement("CREATE TABLE IF NOT EXISTS _info(version TEXT NOT NULL)");
	this->execWriteOnlyStatement("DELETE FROM _info");
	this->execWriteOnlyStatement(std::format("INSERT INTO _info VALUES('{}')", program_version_string).c_str());
}

void DatabaseConnectionPostgreSQL::migrateIfVersionIsNewer(const std::string& program_version_string) {
	try {
		this->execWriteOnlyStatement("CREATE TABLE IF NOT EXISTS _info(version TEXT NOT NULL)");
	}
	catch (std::exception& exception) {
		std::cerr << "[DatabaseConnectionPostgreSQL] Could not create database info table: " << exception.what() << std::endl;
		return;
	}
	char* database_version = db_retrieve_database_version();
	if (database_version[0] == '\0') {
		std::cout << "[DatabaseConnectionPostgreSQL] No database version string found. Writing new one..." << std::endl;
		this->writeDatabaseVersion(program_version_string.c_str());
	}
	else {
		std::string database_version_string = std::string(database_version);
		free(database_version);
		std::cout << database_version_string << " (database) : " << program_version_string << " (server)" << std::endl;
		if (database_version_string < program_version_string) { // Database is outdated.
			std::stringstream migrations;
			if (this->writeMigrations(migrations, database_version_string)) {
				try {
					this->execMultipleWriteOnlyStatements(migrations);
					this->writeDatabaseVersion(program_version_string.c_str());
				}
				catch (std::exception& exception) {
					std::cout << "[DatabaseConnectionPostgreSQL] Exception occured in constructor: " << exception.what() << std::endl;
				}
				std::cout << "[DatabaseConnectionPostgreSQL] Finished doing migrations." << std::endl;
			}
			else {
				std::cout << "[DatabaseConnectionPostgreSQL] No migrations necessary." << std::endl;
				try {
					this->writeDatabaseVersion(program_version_string.c_str());
				}
				catch (std::exception& exception) {
					std::cout << "[DatabaseConnectionPostgreSQL] Exception occured in constructor: " << exception.what() << std::endl;
				}
			}
			migrations.clear();
		}
		else if (database_version_string > program_version_string) // Server is outdated
			std::cout << "[DatabaseConnectionPostgreSQL] WARNING! database version is found to be newer than this server. Issues may occur. Consider updating to a newer version of Fuze Mediaboard." << std::endl;
		else
			std::cout << "[DatabaseConnectionPostgreSQL] Database is up-to-date." << std::endl;
	}
}

bool DatabaseConnectionPostgreSQL::writeMigrations(std::ostream& stream, const std::string& database_version_string) {
	if (database_version_string <= "0.0.5")	goto v0_0_5;
	// If code reaches here, no migrations need to be made
	return false;
v0_0_5:
	stream << "UPDATE permission_collection SET account_id = NULL WHERE account_id = -1;";
	stream << "UPDATE permission_collection SET permission_group_id = NULL WHERE permission_group_id = -1;";
	std::cout << "Finished writing migrations" << std::endl;
	return true; // Migrations were made
}

int DatabaseConnectionPostgreSQL::storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) {
	return db_store_permission_collection(permission_object_id,
								   user_or_group == USER_OR_GROUP::USER ? user_or_group_id : -1,
								   user_or_group == USER_OR_GROUP::GROUP ? user_or_group_id : -1);
}

int DatabaseConnectionPostgreSQL::storePermissionSetting(int permission_collection_id, PERMISSION permission, THREE_STATE_SETTING setting) {
	return db_store_permission_setting(permission_collection_id, static_cast<int>(permission), static_cast<int>(setting));
}

void DatabaseConnectionPostgreSQL::updatePermissionSetting(int permission_setting_id, THREE_STATE_SETTING setting) {
	db_update_permission_setting(permission_setting_id, static_cast<int>(setting));
}
