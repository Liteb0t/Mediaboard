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
import FuzeHttp.State;

export namespace Mediaboard {
struct AudioRelaySlot {
	std::shared_ptr<rtc::Track> track;
	rtc::SSRC ssrc;
};
struct RtcPeer {
	std::function<void(boost::json::object)> send_message;
	int client_id;
	// std::shared_ptr<FuzeHttp::WebsocketSession> session;
	std::shared_ptr<rtc::PeerConnection> connection;
	// sender
	std::shared_ptr<rtc::Track> video_sending_track;
	bool video_sharing_enabled;
	std::shared_ptr<rtc::Track> desktop_audio_track;
	std::shared_ptr<rtc::Track> mic_track;
	bool mic_sharing_enabled = false;
	bool mic_relay_initialized = false;
	// receiver
	std::shared_ptr<rtc::Track> video_receiving_track;
	std::unordered_map<int, AudioRelaySlot> audio_relays;
	bool renegotiation_in_flight = false; // if addAudioRelaySlot is called before request_type == "webrtc_audio_added_answer", connection id is added to the deque next line
	std::deque<int> pending_audio_relay_additions;
};

class Room /*: public FuzeHttp::PermissionManagedObject*/ {
public:
	Room(/*PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, */int id, int board_id/*const ValidatedInput&& input*/)
			: /*PermissionManagedObject(permission_parent, db), */id(id), board_id(board_id) {}

	boost::json::object asJson() const {
		boost::json::array peers_json;
		for (auto& [id, weak_peer] : peers) {
			if (auto peer = weak_peer.lock()) {
				peers_json.push_back({
					{"connection_id", id},
					{"client_id", peer->client_id},
					{"mic_sharing_enabled", peer->mic_sharing_enabled},
					{"video_sharing_enabled", peer->video_sharing_enabled}
				});
			}
		}
		return {
			{"id", this->id},
			{"number_of_peers", this->peers.size()},
			{"peers", peers_json}
		};
	}

	int id;
	int board_id;
	int connection_id_counter = 0;
	unsigned int ssrc_counter = 0;
	std::unordered_map<int, std::weak_ptr<RtcPeer>> peers;
};
}
