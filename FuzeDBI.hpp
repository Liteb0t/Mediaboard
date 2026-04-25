#pragma once

#include <format>
#include <iostream>
#include <string>

#define FUZEDBI_POSTGRES 0
#define FUZEDBI_SQLITE 1

#ifndef FUZEDBI_INTERFACE
#define FUZEDBI_INTERFACE FUZEDBI_POSTGRES
#endif

#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
#include <libpq-fe.h>
#else
#include <sqlite3.h>
#endif

#define DATABASE_PASSWORD_ENVIRONMENT_VARIABLE "FUZE_MEDIABOARD_PASSWORD"

class FuzeDBI {
	enum class PARAMETER_TYPE { CHAR_ARRAY = 0, STRING = 1, UINT8 = 2 };
public:
#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
	FuzeDBI(const std::string& postgresql_user, const std::string& postgresql_host, const unsigned short postgresql_port, const std::string& postgresql_database_name, const std::string& program_version_string) {
		const char* password = getenv(DATABASE_PASSWORD_ENVIRONMENT_VARIABLE);
		std::string libpq_connection_string = std::format("user={} host={} port={} dbname={} password={}", postgresql_user, postgresql_host, postgresql_port, postgresql_database_name, password);
		// std::cout << "[DatabaseConnectionPostgreSQL] libpq connection string: " << libpq_connection_string << std::endl;
		this->db = PQconnectdb(libpq_connection_string.c_str());
		switch (PQstatus(this->db)) {
			case CONNECTION_BAD:
				std::cerr << "[FuzeDBI] Could not connect via libpq: " << PQerrorMessage(this->db) << std::endl;
				break;
			case CONNECTION_OK:
				std::cout << "[FuzeDBI] Connected via libpq successfully" << std::endl;
				break;
			default:
				std::cerr << "[FuzeDBI] Unknown libpq connection status" << std::endl;
				break;
		}
	}
#endif
	template<class ReturnType, class... Args>
	ReturnType exec(const std::string& statement, Args... args) {
#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
		const char* params[sizeof...(args)];
		Oid pg_types[sizeof...(args)];
		int param_i = 0;
		for (std::variant<const char*, std::string, uint8_t> arg : std::initializer_list<std::variant<const char*, std::string, uint8_t>>{ args... }) {
			if (arg.index() == static_cast<int>(PARAMETER_TYPE::CHAR_ARRAY)) {
				params[param_i] = std::get<const char*>(arg);
				pg_types[param_i] = 25;
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::STRING)) {
				params[param_i] = std::get<std::string>(arg).c_str();
				pg_types[param_i] = 25;
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::UINT8)) {
				params[param_i] = std::to_string(std::get<uint8_t>(arg)).c_str();
				pg_types[param_i] = 20;
			}
			else
				throw std::runtime_error("Arg variant unknown");
			param_i++;
		}
		// PGresult* result = PQexec(this->db, statement.c_str());
		PGresult* result = PQexecParams(this->db, statement.c_str(), sizeof...(args), pg_types, params, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);
		std::string error_message;
		switch (status) {
			case PGRES_EMPTY_QUERY:
				// std::cout << "[FuzeDBI] Warning: SQL statement was empty." << std::endl;
			case PGRES_COMMAND_OK: case PGRES_TUPLES_OK:
				break;
			case PGRES_FATAL_ERROR:
				error_message = PQresultErrorMessage(result);
				PQclear(result);
				if (error_message[0] != '\0')
					throw std::runtime_error(error_message);
				else
					throw std::runtime_error(PQerrorMessage(this->db));
				// fprintf(stderr, "[DatabaseConnectionSQLite] SQL error %d: %s\n", ec, error_message);
				break;
			default:
				PQclear(result);
				throw std::runtime_error(std::format("[FuzeDBI] Unknown PWresStatus: {}", PQresStatus(status)));
				break;
		}
		PQclear(result);
#else
		throw std::runtime_error("[FuzeDBI] SQLite interface not implemented");
#endif
	}
private:
#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
	PGconn* db;
#endif
}; // class FuzeDBI
