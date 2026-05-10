// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#pragma once

#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <variant>

// #ifndef FUZEDBI_INTERFACE
// #define FUZEDBI_INTERFACE FUZEDBI_POSTGRES
// #else
// #define FUZEDBI_INTERFACE FUZEDBI_SQLITE
// #endif

#ifdef FUZEDBI_POSTGRES
#include <libpq-fe.h>
#elifdef FUZEDBI_SQLITE
#include <sqlite3.h>
#endif

#define DATABASE_PASSWORD_ENVIRONMENT_VARIABLE "FUZE_MEDIABOARD_PASSWORD"

namespace FuzeDBI {
template<class ReturnType>
class QueryIterator; // Forward declaration
/*
struct ConstructorArgs {
	std::optional<std::string> user;
	std::optional<std::string> host;
	std::optional<unsigned short> port;
	std::optional<std::string> database_name;
	std::optional<std::string> database_filepath;
};
*/
class Connection {
	enum class PARAMETER_TYPE { CHAR_ARRAY = 0, STRING = 1, INT = 2 };
public:
#ifdef FUZEDBI_POSTGRES
	Connection(const std::string& postgresql_user, const std::string& postgresql_host, const unsigned short postgresql_port, const std::string& postgresql_database_name) {
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
#elifdef FUZEDBI_SQLITE
	Connection(const std::string& database_filepath) {
		std::cout << "[FuzeDBI] Connecting to SQLite database at " << database_filepath << std::endl;
		int ec = sqlite3_open(database_filepath.c_str(), &db);
		if (ec) {
			throw std::runtime_error(std::format("[DatabaseConnectionSQLite] Can't open database: {}", sqlite3_errmsg(db)));
			return;
		}
	}
	~Connection() {
		sqlite3_close(this->db);
	}
	// TODO handle strings and escape characters
	std::string pqToSQLiteStatement(const std::string& pq_statement) {
		std::string output;
		output.reserve(pq_statement.length());
		for (size_t i = 0; i < pq_statement.length(); i++) {
			if (pq_statement[i] == '$') {
				std::size_t number_end = pq_statement.find_first_not_of("0123456789", i+1);
				if (number_end == pq_statement.npos) {
					number_end = pq_statement.length();
				}
				if (number_end != i+1) { // there are one or more numeric characters after the $
					output += '?';
					i = number_end - 1;
					continue;
				}
			}
			output += pq_statement[i];
		}
		return output;
	}
#endif
	template<class ReturnType, class... Args>
	ReturnType query(const std::string& statement, Args... args) {
#ifdef FUZEDBI_POSTGRES
		PGresult* result = this->exec(statement, args...);
		ExecStatusType status = PQresultStatus(result);
		std::string error_message;
		switch (status) {
			case PGRES_EMPTY_QUERY:
				// std::cout << "[FuzeDBI] Warning: SQL statement was empty." << std::endl;
			case PGRES_COMMAND_OK: case PGRES_TUPLES_OK:
				if constexpr (!std::is_same_v<ReturnType, void>) {
					ReturnType return_val = getValue<ReturnType>(result);
					PQclear(result);
					return return_val;
				}
				else
					return void();
			case PGRES_FATAL_ERROR:
				error_message = PQresultErrorMessage(result);
				PQclear(result);
				if (error_message[0] != '\0')
					throw std::runtime_error(error_message);
				else
					throw std::runtime_error(PQerrorMessage(this->db));
			default:
				PQclear(result);
				throw std::runtime_error(std::format("[FuzeDBI] Unknown PWresStatus: {}", PQresStatus(status)));
		}
		PQclear(result);
#elifdef FUZEDBI_SQLITE
		// SQLite implementation requires the string to be reformatted. Specifically, the $1 $2 etc parameters should be replaced with question marks.
		std::string formatted_statement = pqToSQLiteStatement(statement);
		// std::cout << "[FuzeDBI] formatted_statement: " <<formatted_statement << std::endl;
		sqlite3_stmt* stmt;
		int ec = sqlite3_prepare_v2(db, formatted_statement.c_str(), -1, &stmt, NULL);
		if (ec != SQLITE_OK) {
			throw std::runtime_error(std::format("[FuzeDBI] SQLite error in statement \"{}\" \n{}", formatted_statement, sqlite3_errmsg(this->db)));
		}
		int param_i = 1;
		for (std::variant<const char*, std::string, int> arg : std::initializer_list<std::variant<const char*, std::string, int>>{ args... }) {
			if (arg.index() == static_cast<int>(PARAMETER_TYPE::CHAR_ARRAY)) {
				sqlite3_bind_text(stmt, param_i, std::get<const char*>(arg), strlen(std::get<const char*>(arg)), SQLITE_TRANSIENT);
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::STRING)) {
				sqlite3_bind_text(stmt, param_i, std::get<std::string>(arg).c_str(), std::get<std::string>(arg).length(), SQLITE_TRANSIENT);
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::INT)) {
				sqlite3_bind_int(stmt, param_i, std::get<int>(arg));
			}
			else
				throw std::runtime_error("Arg variant unknown");
			param_i++;
		}
		switch (sqlite3_step(stmt)) {
			case SQLITE_ROW: case SQLITE_DONE:
				if constexpr (!std::is_same_v<ReturnType, void>) {
					ReturnType return_val = getValue<ReturnType>(stmt);
					// throw std::runtime_error("[FuzeDBI] SQLite interface not implemented");
					return return_val;
				}
				else
					return void();
			default:
				throw std::runtime_error(std::format("[FuzeDBI] SQLite error in statement \"{}\" \n{}", formatted_statement, sqlite3_errmsg(this->db)));
				break;

		}
		sqlite3_finalize(stmt);
#endif
		throw std::runtime_error("[FuzeDBI] Reached end of query function without a return value");
	}
	template<class ReturnType, class... Args>
	QueryIterator<ReturnType> queryRows(const std::string& statement, Args... args) {
#ifdef FUZEDBI_POSTGRES
		PGresult* result = this->exec(statement, args...);
		ExecStatusType status = PQresultStatus(result);
		std::string error_message;
		switch (status) {
			case PGRES_EMPTY_QUERY:
				// std::cout << "[FuzeDBI] Warning: SQL statement was empty." << std::endl;
			case PGRES_COMMAND_OK: case PGRES_TUPLES_OK:
				return QueryIterator<ReturnType>(this, result);
				break;
			case PGRES_FATAL_ERROR:
				error_message = PQresultErrorMessage(result);
				PQclear(result);
				if (error_message[0] != '\0')
					throw std::runtime_error(error_message);
				else
					throw std::runtime_error(PQerrorMessage(this->db));
				break;
			default:
				PQclear(result);
				throw std::runtime_error(std::format("[FuzeDBI] Unknown PWresStatus: {}", PQresStatus(status)));
				break;
		}
#elifdef FUZEDBI_SQLITE
		std::string formatted_statement = pqToSQLiteStatement(statement);
		// std::cout << "[FuzeDBI] formatted_statement: " <<formatted_statement << std::endl;
		sqlite3_stmt* stmt;
		int ec = sqlite3_prepare_v2(db, formatted_statement.c_str(), -1, &stmt, NULL);
		if (ec != SQLITE_OK) {
			throw std::runtime_error(std::format("[FuzeDBI] SQLite error in statement \"{}\" \n{}", formatted_statement, sqlite3_errmsg(this->db)));
		}
		int param_i = 1;
		for (std::variant<const char*, std::string, int> arg : std::initializer_list<std::variant<const char*, std::string, int>>{ args... }) {
			if (arg.index() == static_cast<int>(PARAMETER_TYPE::CHAR_ARRAY)) {
				sqlite3_bind_text(stmt, param_i, std::get<const char*>(arg), strlen(std::get<const char*>(arg)), SQLITE_TRANSIENT);
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::STRING)) {
				sqlite3_bind_text(stmt, param_i, std::get<std::string>(arg).c_str(), std::get<std::string>(arg).length(), SQLITE_TRANSIENT);
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::INT)) {
				sqlite3_bind_int(stmt, param_i, std::get<int>(arg));
			}
			else
				throw std::runtime_error("Arg variant unknown");
			param_i++;
		}
		return QueryIterator<ReturnType>(this, stmt);
#endif
	}
#ifdef FUZEDBI_POSTGRES
	// https://stackoverflow.com/a/79932078/18658154
	template<class ReturnType>
	ReturnType getValue(PGresult* result, int row = 0, int column = 0) {
		return getValueImpl(std::type_identity<ReturnType>{}, result, row, column);
	}
	template<typename T>
	std::optional<T> getValueImpl(std::type_identity<std::optional<T>>, PGresult* result, int row, int column) {
		if (PQgetisnull(result, row, column) == 1) // 1 means null
			return {};
		else
			return this->getValueImpl(std::type_identity<T>{}, result, row, column);
	}
	std::string getValueImpl(std::type_identity<std::string>, PGresult* result, int row, int column) {
		return std::string(PQgetvalue(result, row, column));
	}
	int getValueImpl(std::type_identity<int>, PGresult* result, int row, int column) {
		return std::atoi(PQgetvalue(result, row, column));
	}
	template<class... ReturnTypes>
	std::tuple<ReturnTypes...> getValueImpl(std::type_identity<std::tuple<ReturnTypes...>>, PGresult* result, int row, int column) {
		std::tuple<ReturnTypes...> return_tuple;
		int number_of_columns = PQnfields(result);
		// std::cout << "There are " << number_of_columns << " columns" << std::endl;
		if (number_of_columns != sizeof...(ReturnTypes)) {
			throw std::runtime_error(std::format("[FuzeDBI] The number of result columns {} is different from the number of tuple values {}", number_of_columns, sizeof...(ReturnTypes)));
		}
		fillTuple<0, ReturnTypes...>(return_tuple, result, row);
		return return_tuple;
	}
	template <typename T>
	void getValueImpl(std::type_identity<T>, PGresult* result, int row, int column) {
		throw std::runtime_error("[FuzeDBI] Unknown ReturnType");
	}
#elifdef FUZEDBI_SQLITE
	template<class ReturnType>
	ReturnType getValue(sqlite3_stmt* stmt, int column = 0) {
		// throw std::runtime_error("[FuzeDBI] getValue not implemented for SQLite");
		return getValueImpl(std::type_identity<ReturnType>{}, stmt, column);
	}
	template<typename T>
	std::optional<T> getValueImpl(std::type_identity<std::optional<T>>, sqlite3_stmt* stmt, int column) {
		if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
			// sqlite3_finalize(stmt);
			return {};
		}
		else {
			return this->getValueImpl(std::type_identity<T>{}, stmt, column);
		}
	}
	std::string getValueImpl(std::type_identity<std::string>, sqlite3_stmt* stmt, int column) {
		return std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, column)));
	}
	int getValueImpl(std::type_identity<int>, sqlite3_stmt* stmt, int column) {
		return sqlite3_column_int(stmt, column);
	}
	template<class... ReturnTypes>
	std::tuple<ReturnTypes...> getValueImpl(std::type_identity<std::tuple<ReturnTypes...>>, sqlite3_stmt* stmt, int column) {
		std::tuple<ReturnTypes...> return_tuple;
		int number_of_columns = sqlite3_column_count(stmt);
		// std::cout << "There are " << number_of_columns << " columns" << std::endl;
		// std::cout << "[FuzeDBI] Return tuple size: " << sizeof...(ReturnTypes) << std::endl;
		if (number_of_columns != sizeof...(ReturnTypes)) {
			throw std::runtime_error(std::format("[FuzeDBI] The number of result columns {} is different from the number of tuple values {}", number_of_columns, sizeof...(ReturnTypes)));
		}
		fillTuple<0, ReturnTypes...>(return_tuple, stmt);
		return return_tuple;
	}
	template <typename T>
	void getValueImpl(std::type_identity<T>, sqlite3_stmt* stmt, int column) {
		throw std::runtime_error("[FuzeDBI] Unknown ReturnType");
	}
#endif
private:
#ifdef FUZEDBI_POSTGRES
	PGconn* db;
	template<std::size_t I = 0, typename...TupleParams>
	inline typename std::enable_if<I == sizeof...(TupleParams), void>::type
	fillTuple(std::tuple<TupleParams...>& tuple, PGresult* result, int, int) {
	}
	template<std::size_t I = 0, typename...TupleParams>
	inline typename std::enable_if<I < sizeof...(TupleParams), void>::type
	fillTuple(std::tuple<TupleParams...>& tuple, PGresult* result, int row, int column = 0) {
		auto& entry = std::get<I>(tuple);
		entry = getValue<std::tuple_element_t<I, std::tuple<TupleParams...>>>(result, row, column);
		fillTuple<I + 1>(tuple, result, row, column + 1);
	}
	template<class... Args>
	PGresult* exec(const std::string& statement, Args... args) {
		char* params[sizeof...(args)];
		Oid pg_types[sizeof...(args)];
		int param_i = 0;
		for (std::variant<const char*, std::string, int> arg : std::initializer_list<std::variant<const char*, std::string, int>>{ args... }) {
			if (arg.index() == static_cast<int>(PARAMETER_TYPE::CHAR_ARRAY)) {
				params[param_i] = strdup(std::get<const char*>(arg));
				pg_types[param_i] = 25;
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::STRING)) { // TODO fix string args resulting in formatting error
				params[param_i] = strdup(std::get<std::string>(arg).c_str());
				pg_types[param_i] = 25;
			}
			else if (arg.index() == static_cast<int>(PARAMETER_TYPE::INT)) {
				params[param_i] = strdup(std::to_string(std::get<int>(arg)).c_str());
				pg_types[param_i] = 20;
			}
			else
				throw std::runtime_error("Arg variant unknown");
			param_i++;
		}
		// for (char* param : params) {
		// 	std::cout << "[FuzeDBI] param " << param << std::endl;
		// }
		PGresult* result = PQexecParams(this->db, statement.c_str(), sizeof...(args), pg_types, params, NULL, NULL, 0);
		for (int param_i = 0; param_i < sizeof...(args); param_i++) {
			free(params[param_i]);
		}
		return result;
	}
#elifdef FUZEDBI_SQLITE
	sqlite3* db;
	template<std::size_t I = 0, typename...TupleParams>
	inline typename std::enable_if<I == sizeof...(TupleParams), void>::type
	fillTuple(std::tuple<TupleParams...>& tuple, sqlite3_stmt* stmt, int) {
		// std::cout << "[FuzeDBI] Reached end of tuple" << std::endl;
	}
	template<std::size_t I = 0, typename...TupleParams>
	inline typename std::enable_if<I < sizeof...(TupleParams), void>::type
	fillTuple(std::tuple<TupleParams...>& tuple, sqlite3_stmt* stmt, int column = 0) {
		auto& entry = std::get<I>(tuple);
		entry = getValue<std::tuple_element_t<I, std::tuple<TupleParams...>>>(stmt, column);
		fillTuple<I + 1>(tuple, stmt, column + 1);
	}
#endif
}; // class Connection
#ifdef FUZEDBI_POSTGRES
template<class ReturnType>
class QueryIterator {
public:
	QueryIterator(Connection* db, PGresult* result)
	: db(db),
	result(result),
	number_of_rows(PQntuples(result)) {
	}
	// ~QueryIterator() { PQclear(result); }
	auto operator++() { ++row; return *this; }
	auto begin() { return *this; }
	auto end() { return *this; }
	bool operator!=(const auto& rhs) const {
		return row < rhs.number_of_rows;
	}
	ReturnType operator*() const {
		return db->getValue<ReturnType>(result, row);
	}
private:
	Connection* db;
	PGresult* result;
	size_t row = 0;
	size_t number_of_rows;
};
#elifdef FUZEDBI_SQLITE
template<class ReturnType>
class QueryIterator {
public:
	QueryIterator(Connection* db, sqlite3_stmt* stmt)
	: db(db),
	stmt(stmt) {
		this->stepStatement();
	}
	// ~QueryIterator() { PQclear(result); }
	auto operator++() {
		this->stepStatement();
		return *this;
	}
	auto begin() { return *this; }
	auto end() { return *this; }
	bool operator!=(const auto& rhs) const {
		return !is_done;
	}
	ReturnType operator*() const {
		return db->getValue<ReturnType>(stmt);
	}
private:
	void stepStatement() {
		switch (sqlite3_step(this->stmt)) {
			case SQLITE_DONE:
				is_done = true;
			case SQLITE_ROW:
				break;
			default:
				throw std::runtime_error("[FuzeDBI] SQLite error in QueryIterator");
				break;
		}
	}
	bool is_done = false;
	Connection* db;
	sqlite3_stmt* stmt;
};
#endif
}; // namespace FuzeDBI
