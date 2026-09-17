module;
#include <boost/beast/http/status.hpp>
#include "beast.hpp"
#include <any>
#include <expected>
#include <format>
#include <string>
export module Mediaboard.Resolvers;
import FuzeHttp.Resolver;
import Mediaboard.State;
import Mediaboard.Board;
import Mediaboard.Permission;

using namespace FuzeHttp;

export namespace Mediaboard {

template<PERMISSION permission = PERMISSION::NUMBER_OF_PERMISSIONS> // NUMBER_OF_PERMISSIONS means "none"
struct BoardResolver : Resolver<State*, Board*, std::string> {
	virtual std::expected<void, Response> validateExtraPermission(Board* board, const std::optional<Client>& client) const {return {};}
	std::expected<std::any, FuzeHttp::Response> fetch(State* state, std::string key, const std::optional<Client>& client) const override {
		auto board_res = state->getBoardIfExists(key);
		if (!board_res)
			return std::unexpected(Response{.status=http::status::not_found, .error_message=std::format("Could not find board with slug {}", key)});
		auto board = board_res.value();
		if (!board->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_BOARD))) {
			return std::unexpected(Response{.status=http::status::forbidden, .error_message=std::format("Client does not have permission to access board `{}`", key)});
		}
		if (auto permission_res = validateExtraPermission(board, client); !permission_res)
			return std::unexpected(permission_res.error());
		return board;
	}
};

template<PERMISSION permission>
requires (permission != PERMISSION::NUMBER_OF_PERMISSIONS)
struct BoardResolver<permission> : BoardResolver<PERMISSION::NUMBER_OF_PERMISSIONS> {
	virtual std::expected<void, Response> validateExtraPermission(Board* board, const std::optional<Client>& client) const override {
		if (!board->clientHasPermission(client, static_cast<int>(permission)))
			return std::unexpected(Response{.status=http::status::forbidden, .error_message=std::format("Client does not have permission to perform this action on board `{}`", board->getSlug())});
		return {}; // success
	}
};

struct ThreadResolver : Resolver<State*, Thread*, int, Board*> {
	std::expected<std::any, FuzeHttp::Response> fetch(State* state, int thread_id, Board* board, const std::optional<Client>& client) const override {
		if (!board->threadExists(thread_id))
			return std::unexpected(Response{.status = http::status::not_found, .error_message = std::format("Thread {} was not found.", thread_id)});
		Thread* thread = board->getThread(thread_id);
		if (thread->isDeleted())
			return std::unexpected(Response{.status = http::status::bad_request, .error_message = std::format("Thread {} has been deleted", thread_id)});
		if (!thread->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_THREAD)))
			return std::unexpected(Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."});
		return thread;
	}
};
}
