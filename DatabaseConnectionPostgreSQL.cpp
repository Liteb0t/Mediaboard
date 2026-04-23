#include "DatabaseConnectionPostgreSQL.hpp"
#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include "permission_managed_object.hpp"
#include <format>
#include <iostream>
#include <libpq-fe.h>
#include <sstream>

DatabaseConnectionPostgreSQL::DatabaseConnectionPostgreSQL(const std::string& postgresql_uri, const std::string& program_version_string) {
	this->db = PQconnectdb(postgresql_uri.c_str());
	this->migrateIfVersionIsNewer(program_version_string);
}
DatabaseConnectionPostgreSQL::DatabaseConnectionPostgreSQL(const std::string& postgresql_user, const std::string& postgresql_host, const unsigned short postgresql_port, const std::string& postgresql_database_name, const std::string& program_version_string) {
	const char* password = getenv(DATABASE_PASSWORD_ENVIRONMENT_VARIABLE);
	std::string libpq_connection_string = std::format("user={} host={} port={} dbname={} password={}", postgresql_user, postgresql_host, postgresql_port, postgresql_database_name, password);
	// std::cout << "[DatabaseConnectionPostgreSQL] libpq connection string: " << libpq_connection_string << std::endl;
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

void DatabaseConnectionPostgreSQL::getSecret(char* secret_base64) {
	// TODO change secret on a yearly basis
	PGresult* result =  PQexec(this->db, "SELECT value_base64 FROM _secret");
	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK && PQntuples(result) != 0) {
		std::cerr << "[DatabaseConnectionPostgreSQL] Found secret" << std::endl;
		const char* db_secret = PQgetvalue(result, 0, 0);
		if (strlen(db_secret)+1 == sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)) {
			strcpy(secret_base64, db_secret);
			return;
		}
		std::cerr << "[DatabaseConnectionPostgreSQL] Secret contains unexpected number of characters (expected " << sodium_base64_ENCODED_LEN(128, sodium_base64_VARIANT_URLSAFE)-1 << ", received " << strlen(db_secret) << ')' << std::endl;
		this->execWriteOnlyStatement("DELETE FROM _secret");
	}
	unsigned char random_bytes[crypto_pwhash_SALTBYTES];
	randombytes_buf(random_bytes, crypto_pwhash_SALTBYTES);
	sodium_bin2base64(secret_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE), random_bytes, crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE);
	std::cout << "[DatabaseConnectionPostgreSQL] Generated new secret: " << secret_base64 << std::endl;
	try {
		this->execWriteOnlyStatement("CREATE TABLE IF NOT EXISTS _secret(value_base64 TEXT NOT NULL)");
		this->execWriteOnlyStatement(std::format("INSERT INTO _secret(value_base64) VALUES ('{}')", secret_base64));
	}
	catch (std::exception& exception) {
		std::cerr << "[DatabaseConnectionPostgreSQL] Could not save secret to database: " << exception.what() << std::endl;
	}
	PQclear(result);
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
	PQclear(result);
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

int DatabaseConnectionPostgreSQL::createAccount(const std::string& username, const char* password_hash_hash_base64, const char* intermediate_salt_base64) {
	PGresult* result;
	ExecStatusType status;
	std::string new_account_id;
	result =  PQexec(this->db, "SELECT nextval('account_id_autoincrement')");
	status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK && PQntuples(result) != 0) {
		new_account_id = PQgetvalue(result, 0, 0);
	}
	else {
		std::string error_message = PQresultErrorMessage(result);
		PQclear(result);
		if (error_message[0] != '\0')
			throw std::runtime_error(std::format("An error occured when attempting to create account: {}", error_message));
		else
			throw std::runtime_error("An unknown error occured when attempting to create account.");
	}
	PQclear(result);

	std::cout << "[DatabaseConnectionPostgreSQL] New account ID: " <<new_account_id << std::endl;
	const char* params[4] = {new_account_id.c_str(), username.c_str(), password_hash_hash_base64, intermediate_salt_base64};
	result =  PQexecParams(this->db, "INSERT INTO account(id, username, password_hash_hash_base64, intermediate_salt_base64) VALUES ($1::integer, $2::text, $3::text, $4::text)", 4, NULL, params, NULL, NULL, 0);
	status = PQresultStatus(result);
	if (status == PGRES_COMMAND_OK) {
		PQclear(result);
		return std::stoi(new_account_id);
	}
	else {
		std::string error_message = PQresultErrorMessage(result);
		PQclear(result);
		if (error_message[0] != '\0')
			throw std::runtime_error(std::format("An error occured when attempting to create account: {}", error_message));
		else
			throw std::runtime_error("An unknown error occured when attempting to create account.");
	}
}

int DatabaseConnectionPostgreSQL::getAccountByUsername(const std::string& username) {
	std::string new_account_id;
	PGresult* result;
	ExecStatusType status;
	const char* params[1] = {username.c_str()};
	result =  PQexecParams(this->db, "SELECT id FROM account WHERE username = $1::text", 1, NULL, params, NULL, NULL, 0);;
	status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK) {
		if (PQntuples(result) != 0)
			return std::atoi(PQgetvalue(result, 0, 0));
		else
			return BUILTIN_USERS::PUBLIC;
	}
	else {
		std::string error_message = PQresultErrorMessage(result);
		PQclear(result);
		if (error_message[0] != '\0')
			throw std::runtime_error(std::format("Error occured in getAccountByUsername: {}", error_message));
		else
			throw std::runtime_error("Error occured in getAccountByUsername.");
	}
}

std::string DatabaseConnectionPostgreSQL::getIntermediateSaltFromAccount(int account_id) {
	std::string account_id_ = std::to_string(account_id);
	PGresult* result;
	ExecStatusType status;
	const char* params[1] = {account_id_.c_str()};
	result =  PQexecParams(this->db, "SELECT intermediate_salt_base64 FROM account WHERE id = $1::integer", 1, NULL, params, NULL, NULL, 0);
	status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK) {
		if (PQntuples(result) != 0)
			return PQgetvalue(result, 0, 0);
		else
			return "";
	}
	else {
		std::string error_message = PQresultErrorMessage(result);
		PQclear(result);
		if (error_message[0] != '\0')
			throw std::runtime_error(std::format("Error occured in getIntermediateSaltFromAccount: {}", error_message));
		else
			throw std::runtime_error("Error occured in getIntermediateSaltFromAccount.");
	}
}

bool DatabaseConnectionPostgreSQL::userMatchesPassword(int account_id, const std::string& password_hash_hash_base64) {
	std::string account_id_ = std::to_string(account_id);
	PGresult* result;
	ExecStatusType status;
	const char* params[1] = {password_hash_hash_base64.c_str()};
	result =  PQexecParams(this->db, "SELECT id FROM account WHERE password_hash_hash_base64 = $1::text", 1, NULL, params, NULL, NULL, 0);
	status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK) {
		return (PQntuples(result) != 0);
	}
	else {
		std::string error_message = PQresultErrorMessage(result);
		PQclear(result);
		if (error_message[0] != '\0')
			throw std::runtime_error(std::format("Error occured in userMatchesPassword: {}", error_message));
		else
			throw std::runtime_error("Error occured in userMatchesPassword.");
	}
}

void DatabaseConnectionPostgreSQL::createSession(const std::string& id_base64, int session__account_id, time_t session__created_at) {
	PGresult* result;
	ExecStatusType status;
	std::string session__account_id_str = std::to_string(session__account_id);
	std::string session__created_at_str = std::to_string(session__created_at);
	const char* params[3] = {id_base64.c_str(), session__account_id_str.c_str(), session__created_at_str.c_str()};
	result =  PQexecParams(this->db, "INSERT INTO session(id_base64, account_id, created_at) VALUES ($1::text, $2::integer, $3::integer)", 3, NULL, params, NULL, NULL, 0);
	status = PQresultStatus(result);
	if (status != PGRES_COMMAND_OK) {
		std::string error_message = PQresultErrorMessage(result);
		PQclear(result);
		if (error_message[0] != '\0')
			throw std::runtime_error(std::format("Error occured in createSession: {}", error_message));
		else
			throw std::runtime_error("Error occured in createSession.");
	}
}

void DatabaseConnectionPostgreSQL::deleteSession(const std::string& id_base64) {
	PGresult* result;
	ExecStatusType status;
	const char* params[1] = {id_base64.c_str()};
	result =  PQexecParams(this->db, "DELETE FROM session WHERE id_base64 = $1::text", 1, NULL, params, NULL, NULL, 0);
	status = PQresultStatus(result);
	if (status != PGRES_COMMAND_OK) {
		std::string error_message = PQresultErrorMessage(result);
		PQclear(result);
		if (error_message[0] != '\0')
			throw std::runtime_error(std::format("Error occured in deleteSession: {}", error_message));
		else
			throw std::runtime_error("Error occured in deleteSession.");
	}
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
