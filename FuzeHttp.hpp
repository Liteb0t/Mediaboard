#pragma once
#include "FuzeDBI.hpp"
#include "beast.hpp"
// #include "permission_managed_object.hpp"
#include <sodium.h>
#include <charconv>
#include <string>
#include <string_view>
#include <sys/un.h>
#include <unordered_map>
#include <unordered_set>
#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <variant>
#include <tuple>
#include <vector>
#include <type_traits> // For std::conditional_t

namespace FuzeHttp {
// URL decoding in C http://www.geekhideout.com/urlcode.shtml
char fromHex(char ch);

std::string getDecodedURL(boost::string_view raw_URL);

std::string_view getPathName(const std::string& source_URL);

inline std::string formatCookie(const std::string& session_id) {
	return std::format("Session={}; Path=/; HttpOnly", session_id);
}

typedef const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& Request;

struct Session {
	const int id;
	const int client_id;
	// const std::string key;
	const std::chrono::time_point<std::chrono::system_clock> created_at;
};

struct Invite {
	const int granted_group_id;
	const std::chrono::time_point<std::chrono::system_clock> created_at;
};

struct Client {
	int id;
	std::optional<int> account_id;
	// const std::string session_id;
};

template<typename... Option>
struct Requires {};

struct Response {
	beast::http::status status;
	std::optional<std::unordered_map<std::string, std::string>> headers;
	std::optional<std::string> error_message;
	std::optional<boost::json::object> json;
	std::optional<std::string> body;
};
using Headers = std::unordered_map<std::string, std::string>;

void generatePasswordHashHashBase64(char* password_hash_hash_base64, size_t password_hash_hash_base64_len, const char* password_hash_base64, size_t password_hash_base64_len);

template<typename StateType>
void getSaltBase64(StateType state, const std::string& username, char* salt_base64) {
	unsigned char salt[crypto_pwhash_SALTBYTES];
	std::optional<int> user_id = state->getIdFromUsername(username);
	if (user_id) {
		// std::string intermediate_salt_base64 = state->db->getIntermediateSaltFromAccount(user_id.value());
		std::string intermediate_salt_base64 = "";
		crypto_generichash(
			salt, sizeof salt,
			reinterpret_cast<const unsigned char*>(username.c_str()), username.length(),
			reinterpret_cast<const unsigned char*>(intermediate_salt_base64.c_str()), intermediate_salt_base64.length()
		);
	}
	else {
		std::cout << "Username " << username << " not found. Generating fake salt." << std::endl;
		crypto_generichash(
			salt, sizeof salt,
			reinterpret_cast<const unsigned char*>(username.c_str()), username.length(),
			reinterpret_cast<const unsigned char*>(state->getSecret()), sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)
		);
	}
	sodium_bin2base64(
		salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE),
		salt, crypto_pwhash_SALTBYTES,
		sodium_base64_VARIANT_URLSAFE
	);
}

// https://stackoverflow.com/a/79894118/18658154
// Type Filtering Logic
template<typename... Ts> struct TypeList {};

template<typename T>
struct IsViewArg : std::disjunction<std::is_same<T, int>, std::is_same<T, std::string>, std::is_same<T, Client>> {};

template<typename T>
struct IsPathArg : std::disjunction<IsViewArg<T>, std::is_same<T, const char*>> {};

template<typename In, template<typename> class Pred, typename Out = TypeList<>>
struct Filter;

template<template<typename> class Pred, typename... Out>
struct Filter<TypeList<>, Pred, TypeList<Out...>> { using type = TypeList<Out...>; };

template<typename T, typename... Rest, template<typename> class Pred, typename... Out>
struct Filter<TypeList<T, Rest...>, Pred, TypeList<Out...>> {
	using type = typename std::conditional_t<Pred<T>::value,
	Filter<TypeList<Rest...>, Pred, TypeList<Out..., T>>,
	Filter<TypeList<Rest...>, Pred, TypeList<Out...>>>::type;
};

template<typename StateType, typename T> struct MakeFuncPtr;
template<typename StateType, typename... Args>
struct MakeFuncPtr<StateType, TypeList<Args...>> { using type = Response(*)(StateType, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req, Args...); };

template<typename T> struct MakeArgTuple;
template<typename... Args>
struct MakeArgTuple<TypeList<Args...>> { using type = std::tuple<Args...>; };

// https://stackoverflow.com/a/60882359/18658154
template <typename H>
struct SizeOfT;

template <template <typename...> class TL, typename... Ts>
struct SizeOfT <TL<Ts...>> {
	constexpr static auto value = sizeof...(Ts);
};

template<typename StateType>
class Path {
public:
	virtual size_t getPathSize() const = 0;
	virtual Response executeView(StateType state, Request& req) = 0;
	virtual bool attemptPathMatch(http::verb req_method, std::string_view section, size_t index) = 0;
};

template<typename StateType, class... AllArgs>
class ViewPath : public Path<StateType> {
	// using PathArgs = typename Filter<TypeList<AllArgs...>, IsPathArg>::type;
	using FilteredTypes = typename Filter<TypeList<AllArgs...>, IsViewArg>::type;
	// using FilteredTypes = typename Filter<TypeList<AllArgs...>, IsExtraArg>::type;
	using FuncPtr = typename MakeFuncPtr<StateType, FilteredTypes>::type;
	using ArgTuple = typename MakeArgTuple<FilteredTypes>::type;
	// using ExtrasTuple = typename MakeArgTuple<FilteredExtraTypes>::type;
public:
	constexpr ViewPath(http::verb req_method, FuncPtr v, AllArgs... args)
			: view_func(v),
			req_method(req_method) {
		size_t arg_index, index;
		arg_index = index = 0;
		// this->all_args = std::initializer_list<std::variant<const char*, int, std::string>>{ args... };
		// int all_args_i = 0;
		for (std::variant<const char*, int, std::string, Client> var : std::initializer_list<std::variant<const char*, int, std::string, Client>>{ args... }) {
			this->all_args[index] = var;
			if (var.index() == 3) {
				this->path_starts_at++;
			}
			if (!std::holds_alternative<const char*>(var)) {
				this->pattern_position_to_view_arg_index[index] = arg_index++;
				// std::cout << "Arg is not a char array!" << std::endl;
			}
			index++;
		}
		// std::cout << "Final all_args length: " << this->all_args.size() << std::endl;
	}
	Response executeView(StateType state, Request& req) override {
		if (this->all_args[0].index() == 3) { // There is a Client{} parameter in the view
			std::cout << "Insert client here" << std::endl;
			std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
			if (!client) {
				client = state->createClient();
			}
			this->setArg(0, client);
			//return std::apply(view_func, std::tuple_cat(std::tie(state, req, client), /* extra_args */ view_args));
		}
		//else
			return std::apply(view_func, std::tuple_cat(std::tie(state, req), /* extra_args */ view_args));
	}
	size_t getPathSize() const override {
		return this->all_args.size() - this->path_starts_at;
	}
	bool attemptPathMatch(http::verb req_method, std::string_view section, size_t index) override {
		index += this->path_starts_at;
		// std::cout << "Path starts at " << this->path_starts_at << std::endl;
		if (req_method != this->req_method)
			return false;
		if (index >= this->all_args.size()) {
			// std::cout << "Index " << index << "Is greater than number of args " << this->all_args.size() << std::endl;
			return false;
		}
		// std::cout << ", getting variant";
		const std::variant<const char*, int, std::string, Client> vari = this->all_args[index];

		// std::cout << "Section: \"" << section << "\"";
		if (vari.index() == 0) { // Not a view arg
			std::string str = std::string(std::get<const char*>(vari));
			// std::cout << ", is const \"" << str << '"';
			return str == section;
		}
		else if (vari.index() == 1) { // Integer arg
			int value;
			std::from_chars_result res = std::from_chars(section.data(), section.data() + section.size(), value);
			if (res.ec == std::errc()) {
				// std::cout << ", found value " << value;
				this->setArg(pattern_position_to_view_arg_index[index], value);
				// std::cout << ", returning.";
				return true;
			}
			else {
				// if (res.ec == std::errc::invalid_argument)
					// std::cout << ", this is not a number.\n";
				// if (res.ec == std::errc::result_out_of_range)
					// std::cout << ", this number is larger than an int.\n";
				return false;
			}
		}
		else if (vari.index() == 2) { // String arg
			// std::cout << ", Is string \"" << section << '"';
			this->setArg(pattern_position_to_view_arg_index[index], section);
			// this->setArg<(size_t)0, Functor, int, pattern_position_to_view_arg_index[index]>(pattern_position_to_view_arg_index[index], Functor(), section);
			return true;
		}
		else
			throw std::runtime_error(std::format("Variant {} is not a path arg", vari.index()));
	}
private:
	// https://stackoverflow.com/a/28440573/18658154
	template<std::size_t I = 0, typename T>
	inline typename std::enable_if<I == SizeOfT<FilteredTypes>::value, void>::type
	setArg(int, T) { }

	template<std::size_t I = 0, typename T>
	inline typename std::enable_if<I < SizeOfT<FilteredTypes>::value, void>::type
	setArg(int index, T value) {
		if (index == 0) {
			// Shoutouts to David G https://stackoverflow.com/a/79897965/18658154
			if constexpr (auto& entry = std::get<I>(this->view_args); requires{ entry = value; }) {
				entry = value;
			}
		}
		setArg<I + 1, T>(index-1, value);
	}
	FuncPtr view_func;
	ArgTuple view_args;
	// ExtrasTuple extra_args;
	http::verb req_method;
	// std::vector<std::variant<Client, const char*, int, std::string>> path;
	std::array<std::variant<const char*, int, std::string, Client>, sizeof...(AllArgs)> all_args;
	int path_starts_at = 0;
	std::array<int, sizeof...(AllArgs)> pattern_position_to_view_arg_index; // maps arg Pattern position to View arg position
	// std::tuple<FilteredTypes> view_args;

	// TODO get filtered tuple at compile time
	/*
	// This helper returns a 1-element tuple if T is integral, otherwise an empty tuple.*
	template<typename T>
	auto wrap_if_integral(T&& val) const {
		if constexpr (std::is_integral_v<std::decay_t<T>>) {
			return std::make_tuple(std::forward<T>(val));
		} else {
			return std::tuple<>{};
		}
	}
	template<size_t... Is>
	void invoke_helper(std::index_sequence<Is...>) const {
	    // tuple_cat joins all the 1-element and 0-element tuples into one flat list
	    auto filtered_args = std::tuple_cat(wrap_if_integral(std::get<Is>(view_args))...);
	    std::apply(view_func, filtered_args);
	}
	*/
};

template<typename StateType>
class Controller {
public:
	template</* template<typename...> class RequiresT, class... RequiresArgs, */typename... Types>
	constexpr void addPattern(http::verb req_method, typename MakeFuncPtr<StateType, typename Filter<TypeList<Types...>, IsViewArg>::type>::type view,/* Requires<RequiresArgs...> options = {}, */Types... args) {
	// constexpr void addPattern(http::verb req_method, View<> view, Options options, Types... args) {
		// ViewPath<Types...> vp(view, std::move(args)...);
		all_views.emplace(id_counter);
		views.emplace(id_counter, new ViewPath<StateType, /*RequiresArgs..., */Types...>(req_method, view, std::move(args)...));
		// std::cout << "views[" << id_counter << "] length: " << views.at(id_counter)->path.size() << std::endl;
		id_counter++;
	}

	Response matchPathAndExecute(StateType state, Request& req) {
		if (!req.target().starts_with('/'))
			return Response{.status = http::status::bad_request};

		std::string decoded_url = FuzeHttp::getDecodedURL(req.target());
		std::string_view path_name = FuzeHttp::getPathName(decoded_url);
		std::cout << "[Controller] path_name: " << path_name << std::endl;

		std::unordered_set<int> matched_views = all_views;
		std::string_view section;
		size_t section_index;
		size_t location_start_bound = 0;
		size_t location_end_bound;
		for (section_index = 0; location_start_bound < path_name.size(); section_index++) {
			location_start_bound++;
			location_end_bound = path_name.find('/', location_start_bound);
			if (location_end_bound == std::string_view::npos)
				section = path_name.substr(location_start_bound);
			else
				section = path_name.substr(location_start_bound, location_end_bound - location_start_bound);

			// std::cout << "[" <<section<<"]";
			std::erase_if(matched_views, [this, &req, &section, section_index](const int view_id){
				return this->views.at(view_id)->attemptPathMatch(req.method(), section, section_index) == false;
			});
			// std::cout << '.' << std::endl;
			if (matched_views.size() == 0)
				break;
			else {
				location_start_bound = location_end_bound;
			}
		}
		// Remove matches for URLs shorter than the pattern
		std::erase_if(matched_views, [this, section_index](const int view_id){
			return this->views.at(view_id)->getPathSize() > section_index;
		});
		if (matched_views.size() >= 1) {
			std::cout << "Matching finished: number of matches: " << matched_views.size() << std::endl;
			return views.at(*matched_views.begin())->executeView(state, req);
		}
		else {
			Response res;
			res.status = http::status::not_found;
			std::cout << "No patterns were matched to path_name " << path_name << std::endl;
			return res;
		}
	}
private:
	// StateType state;
	std::unordered_set<int> all_views;
	std::unordered_map<int, Path<StateType>*> views;
	int id_counter = 0;
}; // class Controller

class State {
public: // TODO change to protected if possible
	State(FuzeDBI::Connection* fuze_dbi) : fuze_dbi(fuze_dbi) {}
	std::optional<Client> getClientIfExists(FuzeHttp::Request req) const;
	Client createClient(int account_id = -1);
	std::variant<Client, FuzeHttp::Response> getRequiredClient(FuzeHttp::Request req) const;
	std::unordered_map<int, Client> clients;
	std::unordered_map<std::string /*key_base64*/, Session> sessions;
	std::unordered_map<std::string /*key_base64*/, Invite> invites;
private:
	FuzeDBI::Connection* fuze_dbi;
}; // class State
} // namespace FuzeHttp
