// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#pragma once
#include "Request.hpp"
import FuzeHttp.Core;
import Mediaboard.State;

using namespace FuzeHttp;

FuzeHttp::Response showMainPage(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response createGroup(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response deleteGroup(Mediaboard::State* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response removeMemberFromGroup(Mediaboard::State* state, FuzeHttp::Request req, int group_id, int account_id);
FuzeHttp::Response getGroupMembers(Mediaboard::State* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response getGroups(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response setGroupHeirarchy(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response createMessage(Mediaboard::State* state, FuzeHttp::Request req, Client client);
FuzeHttp::Response createThread(Mediaboard::State* state, FuzeHttp::Request req, Client client);
FuzeHttp::Response deletePost(Mediaboard::State* state, FuzeHttp::Request req, Client client, int thread_id, int message_id_in_thread);
FuzeHttp::Response getThread(Mediaboard::State* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response getThreadPermissions(Mediaboard::State* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response addThreadGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, int thread_id, int group_id);
FuzeHttp::Response addThreadUserPermission(Mediaboard::State* state, FuzeHttp::Request req, int thread_id, int account_id);
FuzeHttp::Response updateThreadGroupPermissions(Mediaboard::State* state, FuzeHttp::Request req, int thread_id, int group_id);
FuzeHttp::Response updateThreadUserPermissions(Mediaboard::State* state, FuzeHttp::Request req, int thread_id, int account_id);
FuzeHttp::Response deleteThreadGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, int thread_id, int group_id);
FuzeHttp::Response deleteThreadUserPermission(Mediaboard::State* state, FuzeHttp::Request req, int thread_id, int account_id);
FuzeHttp::Response getThreads(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response getServerPermissions(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response addServerGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response addServerUserPermission(Mediaboard::State* state, FuzeHttp::Request req, int account_id);
FuzeHttp::Response updateServerGroupPermissions(Mediaboard::State* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response updateServerUserPermissions(Mediaboard::State* state, FuzeHttp::Request req, int account_id);
FuzeHttp::Response deleteServerGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, int group_id);
FuzeHttp::Response deleteServerUserPermission(Mediaboard::State* state, FuzeHttp::Request req, int account_id);
FuzeHttp::Response addGroupsToUser(Mediaboard::State* state, FuzeHttp::Request req, int account_id);
FuzeHttp::Response client(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response getUsers(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response acceptInvite(Mediaboard::State* state, FuzeHttp::Request req, std::string invite_key_base64);
