#pragma once

#include <format>
#include <iostream>
#include <stdexcept>
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

namespace FuzeDBI {
template<class ReturnType>
class QueryIterator; // Forward declaration

class Connection {
	enum class PARAMETER_TYPE { CHAR_ARRAY = 0, STRING = 1, INT = 2 };
public:
#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
	Connection(const std::string& postgresql_user, const std::string& postgresql_host, const unsigned short postgresql_port, const std::string& postgresql_database_name, const std::string& program_version_string) {
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
	ReturnType query(const std::string& statement, Args... args) {
#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
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
		PQclear(result);
#else
		throw std::runtime_error("[FuzeDBI] SQLite interface not implemented");
		// SQLite implementation requires the string to be reformatted. Specifically, the $1 $2 etc parameters should be replaced with question marks.
#endif
	}
	template<class ReturnType, class... Args>
	QueryIterator<ReturnType> queryRows(const std::string& statement, Args... args) {
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
	}
#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
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
		std::cout << "[FuzeDBI] Return tuple size: " << sizeof...(ReturnTypes) << std::endl;
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
#endif
private:
#if FUZEDBI_INTERFACE == FUZEDBI_POSTGRES
	PGconn* db;
	template<std::size_t I = 0, typename...TupleParams>
	inline typename std::enable_if<I == sizeof...(TupleParams), void>::type
	fillTuple(std::tuple<TupleParams...>& tuple, PGresult* result, int, int) {
		std::cout << "[FuzeDBI] Reached end of tuple" << std::endl;
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
#endif
}; // class Connection
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
}; // namespace FuzeDBI
