// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include "rtc/peerconnection.hpp"
#include <rtc/rtc.hpp>
#include "rtc/track.hpp"
#include <boost/asio.hpp>
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
struct RtcPeer;
struct RelaySlot {
	std::shared_ptr<rtc::Track> track;
	rtc::SSRC ssrc;
	bool first_message_was_sent = false; // for debugging
};
class RelayFactoryBase {
public:
    RelayFactoryBase(std::string name) : name(std::move(name)) {}
    virtual ~RelayFactoryBase() = default;
	std::string name;
	virtual RelaySlot addRelayTrack(int sender_connection_id, std::shared_ptr<rtc::PeerConnection> connection, unsigned int ssrc) = 0;
	virtual bool isVideo() const = 0;
};
template<typename RtcDescriptionType>
struct RelayFactory : RelayFactoryBase {
public:
	RelayFactory(std::string name) : RelayFactoryBase(std::move(name)) {}
	RelaySlot addRelayTrack(int sender_connection_id, std::shared_ptr<rtc::PeerConnection> connection, unsigned int ssrc) override {
		RtcDescriptionType media_description = createMediaDescription(sender_connection_id, ssrc);
		auto relay_track = connection->addTrack(media_description);
		relay_track->onMessage([this](rtc::binary message) {}, nullptr); // fixes "no receive callback" warning

		return RelaySlot{relay_track, ssrc};
	}
	bool isVideo() const override {
		if constexpr (std::is_same_v<RtcDescriptionType, rtc::Description::Video>) {
			return true;
		}
		else {
			return false;
		}
	}
private:
	RtcDescriptionType createMediaDescription(int sender_connection_id, unsigned int ssrc) {
		std::string mid = std::format("{}-{}", this->name, sender_connection_id);
		RtcDescriptionType relay_media(mid, rtc::Description::Direction::SendOnly);
		relay_media.addSSRC(ssrc, mid);
		if constexpr (std::is_same_v<RtcDescriptionType, rtc::Description::Audio>) {
			relay_media.addOpusCodec(111);
		}
		else if constexpr (std::is_same_v<RtcDescriptionType, rtc::Description::Video>) {
			relay_media.addVP8Codec(96);
			relay_media.setBitrate(3000);
		}
		return relay_media;
	}
};
const std::string MIC_AUDIO_MID = "ma";
const std::string DESKTOP_AUDIO_MID = "da";
const std::string VIDEO_MID = "v";
struct Relay {
	Relay(RelayFactoryBase* relay_factory_base) : relay_factory(std::unique_ptr<RelayFactoryBase>(relay_factory_base)) {}
	// std::string name;
	std::unique_ptr<RelayFactoryBase> relay_factory;
	std::unordered_map<int, RelaySlot> relays;
	bool initialized = false;
	bool renegotiation_in_flight = false; // if addAudioRelaySlot is called before request_type == "webrtc_audio_added_answer", connection id is added to the deque next line
	std::deque<int> pending_relay_additions;
};
struct RtcPeer {
	RtcPeer() {
		this->relays.emplace(MIC_AUDIO_MID,
			std::make_unique<Relay>(new RelayFactory<rtc::Description::Audio>(MIC_AUDIO_MID)));
		this->relays.emplace(VIDEO_MID,
			std::make_unique<Relay>(new RelayFactory<rtc::Description::Video>(VIDEO_MID)));
		this->relays.emplace(DESKTOP_AUDIO_MID,
			std::make_unique<Relay>(new RelayFactory<rtc::Description::Audio>(DESKTOP_AUDIO_MID)));
	}
	std::function<void(boost::json::object)> send_message;
	int client_id;
	// std::shared_ptr<FuzeHttp::WebsocketSession> session;
	std::shared_ptr<rtc::PeerConnection> connection;
	std::shared_ptr<rtc::DataChannel> keepalive_channel;
	// sender
	std::shared_ptr<rtc::Track> video_sending_track;
	bool video_sharing_enabled = false;
	std::shared_ptr<rtc::Track> desktop_audio_track;
	bool desktop_audio_sharing_enabled = false;
	std::shared_ptr<rtc::Track> mic_track;
	bool mic_sharing_enabled = false;
	// receiver
	std::unordered_map<std::string, std::unique_ptr<Relay>> relays;
	std::expected<Relay*, std::string> getRelayFromString(const std::string& relay_type) {
		if (auto it = this->relays.find(relay_type); it != relays.end())
			return it->second.get();
		else
			return std::unexpected(std::format("[getRelayFromString] relay {} not found for client {}", relay_type, client_id));
	}
};

class Room : public std::enable_shared_from_this<Room> /*: public FuzeHttp::PermissionManagedObject*/ {
public:
	Room(/*PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, */int id/*const ValidatedInput&& input*/)
			: /*PermissionManagedObject(permission_parent, db), */id(id) {}

	enum struct ACTION : int {
		JOIN,
		UPDATE,
		LEAVE
	};
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

	void broadcastPeerList(int own_connection_id, ACTION action) {
		std::lock_guard<std::mutex> lock(mutex);
		boost::json::object data_to_send = {
			{"type", "peers_updated"},
			{"payload", {
				{"action", {{"type", getActionString(action)}, {"peer", own_connection_id}}},
				{"peers", this->asJson().at("peers")}}
			}
		};
		for (auto& weak_peer : this->peers) {
			if (auto strong_peer = weak_peer.second.lock())
				strong_peer->send_message(data_to_send);
		}
	}
	void sendMessageToRelays(int own_connection_id, rtc::binary&& message, const std::string& relay_name) {
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& [peer_id, weak_peer] : this->peers) {
			if (peer_id == own_connection_id) continue;
			if (auto strong_peer = weak_peer.lock()) {
				Relay* relay = strong_peer->getRelayFromString(relay_name).value();
				if (auto it = relay->relays.find(own_connection_id); it != relay->relays.end() && it->second.track->isOpen()) {
					auto rtp = reinterpret_cast<rtc::RtpHeader*>(message.data());
					rtp->setSsrc(it->second.ssrc); // it->second is AudioRelaySlot
					it->second.track->send(message);
					if (!it->second.first_message_was_sent) {
						std::println("[sendMessageToRelays] Initial message sent in a relay_slog for relay {}", relay->relay_factory->name);
						it->second.first_message_was_sent = true;
					}
				}
				else {
					std::println("[sendMessageToRelays] it->second.track->isOpen() is false for relay {}", relay->relay_factory->name); // if exception thrown then relay is relays.end();
				}
			}
		}
	}
	void addRelaySlots(int own_connection_id, const std::string MID) {
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& [peer_id, weak_peer] : this->peers) {
			if (peer_id == own_connection_id) continue;
			if (auto strong_peer = weak_peer.lock())
				addRelaySlotUnlocked(own_connection_id, strong_peer, strong_peer->getRelayFromString(MID).value());
		}
	}

	void addRelaySlot(int sender_connection_id, std::shared_ptr<RtcPeer> receiver_peer, Relay* relay) {
		std::lock_guard<std::mutex> lock(mutex);
		addRelaySlotUnlocked(sender_connection_id, receiver_peer, relay);
	}

	void addReceivingRelayTracks(int own_connection_id, std::shared_ptr<RtcPeer> own_peer_ptr) {
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& [sender_connection_id, weak_sender] : this->peers) {
			if (sender_connection_id == own_connection_id)
				continue;
			if (auto sender_peer = weak_sender.lock()) {
				for (auto& [relay_type, relay] : sender_peer->relays) {
					if (relay->initialized) {
						Relay* own_relay = own_peer_ptr->getRelayFromString(relay_type).value();
						addRelayTrackToRelay(own_relay, sender_connection_id, own_peer_ptr->connection);
					}
				}
			}
		}
	}
	void addRelaySlotDependingOnMid(const std::string& relay_name, std::shared_ptr<RtcPeer> peer_ptr) {
		std::lock_guard<std::mutex> lock(mutex);
		auto relay_maybe = peer_ptr->getRelayFromString(relay_name);
		if (!relay_maybe)
			throw relay_maybe.error();

		Relay* relay_ptr = relay_maybe.value();
		relay_ptr->renegotiation_in_flight = false;
		if (!relay_ptr->pending_relay_additions.empty()) {
			int next = relay_ptr->pending_relay_additions.front();
			relay_ptr->pending_relay_additions.pop_front();
			addRelaySlotUnlocked(next, peer_ptr, relay_ptr);
			// if (relay_ptr->name == MIC_AUDIO_MID)
			// 	addRelaySlot<rtc::Description::Audio>(next, peer_ptr, relay_ptr);
			// else if (relay_ptr->name == VIDEO_MID)
			// 	addRelaySlot<rtc::Description::Video>(next, peer_ptr, relay_ptr);
			// else if (relay_ptr->name == DESKTOP_AUDIO_MID)
			// 	addRelaySlot<rtc::Description::Audio>(next, peer_ptr, relay_ptr);
		}
	}

	int id;
	int connection_id_counter = 0;
	unsigned int ssrc_counter = 0;
	std::unordered_map<int, std::weak_ptr<RtcPeer>> peers;
	mutable std::mutex mutex;
private:
	void addRelayTrackToRelay(Relay* relay, int sender_connection_id, std::shared_ptr<rtc::PeerConnection> connection) {
		RelaySlot relay_slot = relay->relay_factory->addRelayTrack(sender_connection_id, connection, ++ssrc_counter);
		if (relay->relay_factory->isVideo()) {
			// [AI glasnost] this section generated by Claude Sonnet 5
			relay_slot.track->setMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
			auto self = std::static_pointer_cast<Mediaboard::Room>(this->shared_from_this());
			relay_slot.track->onOpen([self, sender_connection_id]() {
				std::print("[addRelayTrack] Attempting to request keyframe...");
				auto it = self->peers.find(sender_connection_id);
				if (it != self->peers.end()) {
					if (auto sender = it->second.lock()) {
						std::println("Requesting keyframe.");
						if (sender->video_sending_track && sender->video_sending_track->isOpen())
							sender->video_sending_track->requestKeyframe();
						else
							std::println(std::cerr, "[Room::addRelayTrackToRelay] sender->video_sending_track is null or closed for peer {}", it->first);
					}
				}
			});
			/*
			if constexpr (std::is_same_v<DescriptionType, rtc::Description::Video>) {
				relay_track->onOpen([this, sender_connection_id, relay_track]() {
					auto self = std::static_pointer_cast<Mediaboard::Room>(this->shared_from_this());
					auto attempts = std::make_shared<int>(5);
					auto timer = std::make_shared<boost::asio::steady_timer>(this->getExecutor()); // How do i pass executor into here? from where?
					auto tick = std::make_shared<std::function<void()>>();
					*tick = [self, sender_connection_id, relay_track, attempts, timer, tick]() {
						auto it = self->peers.find(sender_connection_id);
						if (it == self->peers.end()) return;
						std::println("[tick] Attempting to request keyframe");
						if (auto sender = it->second.lock())
							sender->video_sending_track->requestKeyframe();
						if (--(*attempts) > 0 && relay_track->isOpen()) {
							timer->expires_after(std::chrono::milliseconds(400));
							timer->async_wait([tick](auto ec){ if (!ec) (*tick)(); });
						}
					};
					(*tick)();
				});
			}*/
			// [AI glasnost] end AI-generated section
		}
		relay->relays.emplace(sender_connection_id, std::move(relay_slot));
	}
	void addRelaySlotUnlocked(int sender_connection_id, std::shared_ptr<RtcPeer> receiver_peer, Relay* relay) {
		if (relay->renegotiation_in_flight) {
			relay->pending_relay_additions.push_back(sender_connection_id);
			return;
		}
		relay->renegotiation_in_flight = true;
		addRelayTrackToRelay(relay, sender_connection_id, receiver_peer->connection);

		// FuzeHttp::WebsocketSession* receiver_session = static_cast<FuzeHttp::WebsocketSession*>(receiver_peer->owner_session);
		receiver_peer->connection->onLocalDescription([receiver_peer, sender_connection_id, relay](rtc::Description description) {
			if (description.type() != rtc::Description::Type::Offer)
				return; // ignore the answer echo, if any
			receiver_peer->send_message({
				{"type", "webrtc_relay_added"},
				{"payload", {
					{"relay_name", relay->relay_factory->name},
					{"sender_connection_id", sender_connection_id},
					{"description", {
						{"type", description.typeString()},
						{"sdp", std::string(description)}
					}}
				}}
			});
		});
		receiver_peer->connection->setLocalDescription();
	}
	std::string getActionString(ACTION action) {
		if (action == ACTION::JOIN)
			return "join";
		else if (action == ACTION::UPDATE)
			return "update";
		else if (action == ACTION::LEAVE)
			return "leave";
		else
			throw "action not recognised";
	}
};
}
