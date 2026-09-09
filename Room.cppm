module;
#include "rtc/peerconnection.hpp"
#include <rtc/rtc.hpp>
#include "rtc/track.hpp"
#include <boost/json.hpp>
#include <ctime>
#include <expected>
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
	const std::string MIC_AUDIO_MID = "ma";
	const std::string DESKTOP_AUDIO_MID = "da";
	const std::string VIDEO_MID = "v";
struct RelaySlot {
	std::shared_ptr<rtc::Track> track;
	rtc::SSRC ssrc;
};
struct Relay {
	std::string name;
	std::unordered_map<int, RelaySlot> relays;
	bool initialized = false;
	bool renegotiation_in_flight = false; // if addAudioRelaySlot is called before request_type == "webrtc_audio_added_answer", connection id is added to the deque next line
	std::deque<int> pending_relay_additions;
};
struct RtcPeer {
	std::function<void(boost::json::object)> send_message;
	int client_id;
	// std::shared_ptr<FuzeHttp::WebsocketSession> session;
	std::shared_ptr<rtc::PeerConnection> connection;
	// sender
	std::shared_ptr<rtc::Track> video_sending_track;
	bool video_sharing_enabled = false;
	std::shared_ptr<rtc::Track> desktop_audio_track;
	bool desktop_audio_sharing_enabled = false;
	std::shared_ptr<rtc::Track> mic_track;
	bool mic_sharing_enabled = false;
	// receiver
	Relay mic_relay{.name=MIC_AUDIO_MID};
	Relay video_relay{.name=VIDEO_MID};
	Relay desktop_audio_relay{.name=DESKTOP_AUDIO_MID};
	std::expected<Relay*, std::string> getRelayFromString(const std::string& relay_type) {
		if (relay_type == MIC_AUDIO_MID)
			return &(this->mic_relay);
		else if (relay_type == VIDEO_MID)
			return &(this->video_relay);
		else if (relay_type == DESKTOP_AUDIO_MID)
			return &(this->desktop_audio_relay);
		else
			return std::unexpected(std::format("[getRelayFromString] relay_type {} not recognised", relay_type));
	}
};

class Room /*: public FuzeHttp::PermissionManagedObject*/ {
public:
	Room(/*PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, */int id/*const ValidatedInput&& input*/)
			: /*PermissionManagedObject(permission_parent, db), */id(id) {}

	boost::json::object asJson() const {
		boost::json::array peers_json;
		for (auto& [id, weak_peer] : peers) {
			if (auto peer = weak_peer.lock()) {
				peers_json.push_back({
					{"connection_id", id},
					{"client_id", peer->client_id},
					{"desktop_audio_sharing_enabled", peer->desktop_audio_sharing_enabled},
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
	int connection_id_counter = 0;
	unsigned int ssrc_counter = 0;
	std::unordered_map<int, std::weak_ptr<RtcPeer>> peers;
};
}
