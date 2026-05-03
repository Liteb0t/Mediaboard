// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response showMainPage(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response createGroup(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response getGroupMembers(shared_state* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response getGroups(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response setGroupHeirarchy(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response createMessage(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client);
FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client);
FuzeHttp::Response getThread(shared_state* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response getThreadPermissions(shared_state* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response addThreadGroupPermission(shared_state* state, FuzeHttp::Request req, int thread_id, int group_id);
FuzeHttp::Response addThreadUserPermission(shared_state* state, FuzeHttp::Request req, int thread_id, int account_id);
FuzeHttp::Response updateThreadGroupPermissions(shared_state* state, FuzeHttp::Request req, int thread_id, int group_id);
FuzeHttp::Response updateThreadUserPermissions(shared_state* state, FuzeHttp::Request req, int thread_id, int account_id);
FuzeHttp::Response getThreads(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response getServerPermissions(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response addServerGroupPermission(shared_state* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response addServerUserPermission(shared_state* state, FuzeHttp::Request req, int account_id);
FuzeHttp::Response updateServerGroupPermissions(shared_state* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response updateServerUserPermissions(shared_state* state, FuzeHttp::Request req, int account_id);
FuzeHttp::Response addGroupsToUser(shared_state* state, FuzeHttp::Request req, int account_id);
FuzeHttp::Response client(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response getUsers(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response acceptInvite(shared_state* state, FuzeHttp::Request req, std::string invite_key_base64);
FuzeHttp::Response getMedia(shared_state* state, FuzeHttp::Request req, std::string file_name);
FuzeHttp::Response getThumbnail(shared_state* state, FuzeHttp::Request req, std::string file_name);
FuzeHttp::Response showThread(shared_state* state, FuzeHttp::Request req, int thread_id);
