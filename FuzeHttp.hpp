#pragma once
#include <charconv>
#include <string>
#include <sys/un.h>
#include <unordered_map>
#include <unordered_set>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <sstream>
#include <variant>
#include <tuple>
#include <vector>
#include <type_traits> // For std::conditional_t

namespace FuzeHttp {
// https://stackoverflow.com/a/79894118/18658154
// Type Filtering Logic
template<typename... Ts> struct TypeList {};

template<typename T>
struct IsViewArg : std::disjunction<std::is_same<T, int>, std::is_same<T, std::string>> {};

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
struct MakeFuncPtr<StateType, TypeList<Args...>> { using type = void(*)(StateType, Args...); };

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
	virtual void executeView(StateType state) const = 0;
	virtual bool attemptPathMatch(std::string_view section, size_t index) = 0;
};

template<typename StateType, class... AllArgs>
class ViewPath : public Path<StateType> {
	using FilteredTypes = typename Filter<TypeList<AllArgs...>, IsViewArg>::type;
	using FuncPtr = typename MakeFuncPtr<StateType, FilteredTypes>::type;
	using ArgTuple = typename MakeArgTuple<FilteredTypes>::type;
public:
	constexpr ViewPath(FuncPtr v, AllArgs... args)
			: view_func(v) {
		size_t arg_index, index;
		arg_index = index = 0;
		this->path = std::initializer_list<std::variant<const char*, int, std::string>>{ args... };
		for (std::variant<const char*, int, std::string> s : std::initializer_list<std::variant<const char*, int, std::string>>{ args... }) {
			// this->path.push_back(s);
			if (std::holds_alternative<const char*>(s))
				std::cout << "Found char array in list: " << std::get<const char*>(s) << std::endl;
			else {
				this->pattern_position_to_view_arg_index[index] = arg_index++;
				std::cout << "Arg is not a char array!" << std::endl;
			}
			index++;
		}
		std::cout << "Final path length: " << this->path.size() << std::endl;
	}
	void executeView(StateType state) const override {
		std::apply(view_func, std::tuple_cat(std::tie(state), view_args));
	}
	size_t getPathSize() const override {
		return this->path.size();
	}
	bool attemptPathMatch(std::string_view section, size_t index) override {
		// const Path* view = views.at(view_id);
		std::cout << "Pattern " << index << '/' << this->path.size() << ' ';
		// std::forward_list<std::variant<std::string, int>>::const_iterator pattern = *view;
		if (index >= this->path.size()) {
			std::cout << ", erasing." << std::endl;
			return false;
		}
		std::cout << ", getting variant";
		const std::variant<const char*, int, std::string> vari = this->path[index];

		std::cout << "Section: \"" << section << "\"";
		if (vari.index() == 0) { // Not a view arg
			std::string str = std::string(std::get<const char*>(vari));
			std::cout << ", is const \"" << str << '"';
			if (str == section) {
				std::cout << ", matches!";
				return true;
			}
			else {
				std::cout << ", " << section << " does not match " << str << std::endl;
				return false;
			}
		}
		else if (vari.index() == 1) { // Integer arg
			int value;
			std::from_chars_result res = std::from_chars(section.data(), section.data() + section.size(), value);
			if (res.ec == std::errc()) {
				std::cout << ", found value " << value;
				this->setArg(pattern_position_to_view_arg_index[index], value);
				std::cout << ", returning.";
				return true;
			}
			else {
				if (res.ec == std::errc::invalid_argument)
					std::cout << ", this is not a number.\n";
				if (res.ec == std::errc::result_out_of_range)
					std::cout << ", this number is larger than an int.\n";
				return false;
			}
		}
		else { // String arg
			std::cout << ", Is string \"" << section << '"';
			this->setArg(pattern_position_to_view_arg_index[index], section);
			// this->setArg<(size_t)0, Functor, int, pattern_position_to_view_arg_index[index]>(pattern_position_to_view_arg_index[index], Functor(), section);
			return true;
		}
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
	std::vector<int> test;
	std::vector<std::variant<const char*, int, std::string>> path;
	// std::unordered_set<int, int> pattern_position_to_view_arg_index; // maps arg Pattern position to View arg position
	std::array<int, sizeof...(AllArgs)> pattern_position_to_view_arg_index; // maps arg Pattern position to View arg position
	// std::tuple<FilteredTypes> view_args;

	// This helper returns a 1-element tuple if T is integral, otherwise an empty tuple.
	template<typename T>
	auto wrap_if_integral(T&& val) const {
		if constexpr (std::is_integral_v<std::decay_t<T>>) {
			return std::make_tuple(std::forward<T>(val));
		} else {
			return std::tuple<>{};
		}
	}

	// TODO get filtered tuple at compile time
	// template<size_t... Is>
	// void invoke_helper(std::index_sequence<Is...>) const {
	//     // tuple_cat joins all the 1-element and 0-element tuples into one flat list
	//     auto filtered_args = std::tuple_cat(wrap_if_integral(std::get<Is>(view_args))...);
	//     std::apply(view_func, filtered_args);
	// }
};

template<typename StateType>
class Controller {
public:

	template<typename... Types>
	constexpr void addPattern(typename MakeFuncPtr<StateType, typename Filter<TypeList<Types...>, IsViewArg>::type>::type view, Types... args) {
		// ViewPath<Types...> vp(view, std::move(args)...);
		all_views.emplace(id_counter);
		views.emplace(id_counter, new ViewPath<StateType, Types...>(view, std::move(args)...));
		// std::cout << "views[" << id_counter << "] length: " << views.at(id_counter)->path.size() << std::endl;
		id_counter++;
	}

	void matchPathAndExecute(StateType state, std::string_view location) {
		// boost::algorithm::trim_left(location_);
		// location = location.substr(location.starts_with('/') ? 1 : 0,
		// 						   location.ends_with('/') ? location.length() - 1 : location.length());
		// boost::algorithm::trim_right(location_);
		std::cout << "[Controller] location: " << location << std::endl;
		// std::list<std::forward_list<std::variant<std::string, int>>::const_iterator> matched_views;
		std::unordered_set<int> matched_views = all_views;
		std::string_view section;
		size_t i, location_start_bound = location.starts_with('/') ? 1 : 0, location_end_bound;
		for (i = 0; (location_end_bound = location.find('/', location_start_bound)) != std::string_view::npos; i++) {
			section = location.substr(location_start_bound, location_end_bound - location_start_bound);
			std::cout << "[" <<section<<"]";
			// for (std::list<std::forward_list<std::variant<std::string, int>>::const_iterator>::const_iterator view = matched_views.begin(); view != matched_views.end(); view++) {
			for (int view_id : matched_views) {
				bool is_match = views.at(view_id)->attemptPathMatch(section, i);
				if (!is_match) {
					matched_views.erase(view_id);
				}
			}
			std::cout << '.' << std::endl;
			if (matched_views.size() == 0)
				break;
			else
				location_start_bound = location_end_bound + 1;
		}
		// Remove matches for URLs shorter than the pattern
		for (int view_id : matched_views) {
			if (views.at(view_id)->getPathSize() > i) {
				std::cout << "View " << view_id << " path size " << views.at(view_id)->getPathSize() << " is longer than " << i << ". Removing." << std::endl;
				matched_views.erase(view_id);
			}
		}
		if (matched_views.size() >= 1) {
			std::cout << "Matching finished: number of matches: " << matched_views.size() << std::endl;
			views.at(*matched_views.begin())->executeView(state);
		}
		else {
			std::cout << "No patterns were matched to location " << location << std::endl;
		}
	}
private:
	// StateType state;
	std::unordered_set<int> all_views;
	std::unordered_map<int, Path<StateType>*> views;
	int id_counter = 0;
}; // class Controller
} // namespace FuzeHttp
