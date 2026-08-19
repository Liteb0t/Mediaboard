module;
#include "rtc/peerconnection.hpp"
#include <rtc/rtc.hpp>
#include "rtc/track.hpp"
#include <boost/json.hpp>
#include <ctime>
#include <iostream>
#include <map>
#include <print>
#include <string>
#include <bits/unique_ptr.h>
#include <unordered_set>
export module Mediaboard.Room;

// import Mediaboard.Thread;
// import Mediaboard.Permission;
// import FuzeDBI;
import FuzeHttp.PermissionObject;

export namespace Mediaboard {
struct RtcPeer {
	std::shared_ptr<rtc::PeerConnection> connection;
	std::shared_ptr<rtc::Track> track;
};
class Room /*: public FuzeHttp::PermissionManagedObject*/ {
public:
	Room(/*PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, */int id, int board_id/*const ValidatedInput&& input*/)
			: /*PermissionManagedObject(permission_parent, db), */id(id), board_id(board_id) {}
	boost::json::object asJson() const {
		boost::json::object room_json = {
			{"id", this->id}
		};
		return room_json;
	}
	int id;
	int board_id;
	int connection_id_counter = 0;
	std::unordered_map<int, std::shared_ptr<RtcPeer>> senders;
	std::unordered_map<int, std::shared_ptr<RtcPeer>> receivers;
};
}
