#include "chat/CallManager.h"

#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/audio_options.h>
#include <api/create_peerconnection_factory.h>
#include <api/jsep.h>
#include <api/make_ref_counted.h>
#include <api/media_stream_interface.h>
#include <api/set_local_description_observer_interface.h>
#include <api/set_remote_description_observer_interface.h>

#include <QJsonValue>
#include <QMetaObject>

#include <utility>
#include <vector>

namespace devicehub {

/// webrtc::PeerConnectionObserver adapter — every callback fires on
/// WebRTC's signaling thread and immediately hops back to CallManager's
/// own (GUI) thread via QMetaObject::invokeMethod before touching any
/// shared state; see the class doc comment on CallManager for why that's
/// safe even if CallManager is destroyed mid-flight.
class CallManager::PeerObserver : public webrtc::PeerConnectionObserver {
public:
    PeerObserver(CallManager& manager, QString peerLogin) : manager_(manager), peerLogin_(std::move(peerLogin)) {}

    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState /*newState*/) override {}
    void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> /*dataChannel*/) override {}
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState /*newState*/) override {}

    void OnIceCandidate(const webrtc::IceCandidate* candidate) override {
        const QJsonObject payload{
            {"kind", QStringLiteral("ice")},
            {"candidate", QString::fromStdString(candidate->ToString())},
            {"sdpMid", QString::fromStdString(candidate->sdp_mid())},
            {"sdpMLineIndex", candidate->sdp_mline_index()},
        };
        CallManager* manager = &manager_;
        const QString peerLogin = peerLogin_;
        QMetaObject::invokeMethod(
            manager, [manager, peerLogin, payload] { manager->handleLocalIceCandidate(peerLogin, payload); },
            Qt::QueuedConnection);
    }

private:
    CallManager& manager_;
    QString peerLogin_;
};

class CallManager::LocalDescriptionSetObserver : public webrtc::SetLocalDescriptionObserverInterface {
public:
    LocalDescriptionSetObserver(CallManager& manager, QString peerLogin)
        : manager_(manager), peerLogin_(std::move(peerLogin)) {}

    void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
        CallManager* manager = &manager_;
        const QString peerLogin = peerLogin_;
        const bool ok = error.ok();
        const QString message = QString::fromUtf8(error.message());
        QMetaObject::invokeMethod(
            manager,
            [manager, peerLogin, ok, message] { manager->handleLocalDescriptionSet(peerLogin, ok, message); },
            Qt::QueuedConnection);
    }

private:
    CallManager& manager_;
    QString peerLogin_;
};

class CallManager::RemoteDescriptionSetObserver : public webrtc::SetRemoteDescriptionObserverInterface {
public:
    RemoteDescriptionSetObserver(CallManager& manager, QString peerLogin)
        : manager_(manager), peerLogin_(std::move(peerLogin)) {}

    void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
        CallManager* manager = &manager_;
        const QString peerLogin = peerLogin_;
        const bool ok = error.ok();
        const QString message = QString::fromUtf8(error.message());
        QMetaObject::invokeMethod(
            manager,
            [manager, peerLogin, ok, message] { manager->handleRemoteDescriptionSet(peerLogin, ok, message); },
            Qt::QueuedConnection);
    }

private:
    CallManager& manager_;
    QString peerLogin_;
};

CallManager::CallManager(ChatClient& chatClient, QObject* parent) : QObject(parent), chatClient_(chatClient) {
    connect(&chatClient_, &ChatClient::callRosterReceived, this, &CallManager::onCallRosterReceived);
    connect(&chatClient_, &ChatClient::callPeerJoined, this, &CallManager::onCallPeerJoined);
    connect(&chatClient_, &ChatClient::callPeerLeft, this, &CallManager::onCallPeerLeft);
    connect(&chatClient_, &ChatClient::callSignalReceived, this, &CallManager::onCallSignalReceived);
}

CallManager::~CallManager() {
    leaveCall();
}

void CallManager::joinCall() {
    if (inCall_) {
        return;
    }
    ensureFactory();
    inCall_ = true;
    chatClient_.joinCall();
}

void CallManager::leaveCall() {
    if (!inCall_) {
        return;
    }
    inCall_ = false;
    chatClient_.leaveCall();
    for (auto& [login, entry] : peers_) {
        if (entry.connection) {
            entry.connection->Close();
        }
    }
    peers_.clear();
}

void CallManager::ensureFactory() {
    if (peerConnectionFactory_) {
        return;
    }

    networkThread_ = webrtc::Thread::CreateWithSocketServer();
    networkThread_->Start();
    workerThread_ = webrtc::Thread::Create();
    workerThread_->Start();
    signalingThread_ = webrtc::Thread::Create();
    signalingThread_->Start();

    // Playout sink left unset and pushCapturedAudio() is never called —
    // real AudioInputDevice/AudioOutputDevice wiring is deferred (see
    // class doc comment); peers negotiate correctly but carry silence.
    audioDeviceModule_ = webrtc::make_ref_counted<CallAudioDeviceModule>(CallAudioDeviceModule::PlayoutSink{});

    peerConnectionFactory_ = webrtc::CreatePeerConnectionFactory(
        networkThread_.get(), workerThread_.get(), signalingThread_.get(), audioDeviceModule_,
        webrtc::CreateBuiltinAudioEncoderFactory(), webrtc::CreateBuiltinAudioDecoderFactory(),
        /*video_encoder_factory=*/nullptr, /*video_decoder_factory=*/nullptr, /*audio_mixer=*/nullptr,
        /*audio_processing=*/nullptr);

    const webrtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource =
        peerConnectionFactory_->CreateAudioSource(webrtc::AudioOptions());
    localAudioTrack_ = peerConnectionFactory_->CreateAudioTrack("call-audio0", audioSource.get());
}

CallManager::PeerConnectionEntry* CallManager::ensurePeerConnection(const QString& peerLogin) {
    const std::string key = peerLogin.toStdString();
    if (const auto it = peers_.find(key); it != peers_.end()) {
        return &it->second;
    }

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer stunServer;
    stunServer.urls.push_back("stun:stun.l.google.com:19302");
    config.servers.push_back(stunServer);

    auto observer = std::make_unique<PeerObserver>(*this, peerLogin);
    webrtc::PeerConnectionDependencies dependencies(observer.get());

    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::PeerConnectionInterface>> result =
        peerConnectionFactory_->CreatePeerConnectionOrError(config, std::move(dependencies));
    if (!result.ok()) {
        emit callError(QStringLiteral("Failed to create peer connection to %1: %2")
                           .arg(peerLogin, QString::fromUtf8(result.error().message())));
        return nullptr;
    }

    PeerConnectionEntry entry;
    entry.connection = result.value();
    entry.observer = std::move(observer);

    if (localAudioTrack_) {
        const webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> addTrackResult =
            entry.connection->AddTrack(localAudioTrack_, std::vector<std::string>{"call-stream"});
        if (!addTrackResult.ok()) {
            emit callError(QStringLiteral("Failed to attach local audio to %1: %2")
                               .arg(peerLogin, QString::fromUtf8(addTrackResult.error().message())));
        }
    }

    return &peers_.emplace(key, std::move(entry)).first->second;
}

void CallManager::closePeerConnection(const QString& peerLogin) {
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end()) {
        return;
    }
    if (it->second.connection) {
        // Synchronous per webrtc's contract: no further observer
        // callbacks occur once Close() returns, so it's then safe to
        // destroy the observer (via peers_.erase() below).
        it->second.connection->Close();
    }
    peers_.erase(it);
}

void CallManager::negotiateLocal(const QString& peerLogin) {
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end() || !it->second.connection) {
        return;
    }
    const webrtc::scoped_refptr<LocalDescriptionSetObserver> observer =
        webrtc::make_ref_counted<LocalDescriptionSetObserver>(*this, peerLogin);
    it->second.connection->SetLocalDescription(observer);
}

void CallManager::handleLocalDescriptionSet(const QString& peerLogin, bool ok, const QString& errorMessage) {
    if (!ok) {
        emit callError(QStringLiteral("Local description failed for %1: %2").arg(peerLogin, errorMessage));
        return;
    }
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end() || !it->second.connection) {
        return;
    }
    const webrtc::SessionDescriptionInterface* description = it->second.connection->local_description();
    if (description == nullptr) {
        return;
    }
    const QString kind =
        description->GetType() == webrtc::SdpType::kOffer ? QStringLiteral("offer") : QStringLiteral("answer");
    const QJsonObject payload{{"kind", kind}, {"sdp", QString::fromStdString(description->ToString())}};
    chatClient_.sendCallSignal(peerLogin, payload);
}

void CallManager::handleRemoteDescriptionSet(const QString& peerLogin, bool ok, const QString& errorMessage) {
    if (!ok) {
        emit callError(QStringLiteral("Remote description failed for %1: %2").arg(peerLogin, errorMessage));
        return;
    }
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end() || !it->second.connection) {
        return;
    }
    // We just applied a remote offer — respond with an auto-created
    // answer. If instead we just applied a remote answer to our own
    // earlier offer, signaling state is back to stable and there's
    // nothing further to do.
    if (it->second.connection->signaling_state() == webrtc::PeerConnectionInterface::kHaveRemoteOffer) {
        negotiateLocal(peerLogin);
    }
}

void CallManager::handleLocalIceCandidate(const QString& peerLogin, const QJsonObject& payload) {
    chatClient_.sendCallSignal(peerLogin, payload);
}

void CallManager::onCallRosterReceived(const QStringList& participants) {
    // New-joiner-always-offers rule: we just joined, so we initiate to
    // everyone already present.
    for (const QString& peerLogin : participants) {
        if (ensurePeerConnection(peerLogin) != nullptr) {
            negotiateLocal(peerLogin);
        }
    }
}

void CallManager::onCallPeerJoined(const QString& login) {
    // Purely informational — the new joiner initiates, we just wait for
    // their offer via onCallSignalReceived().
    emit participantJoined(login);
}

void CallManager::onCallPeerLeft(const QString& login) {
    closePeerConnection(login);
    emit participantLeft(login);
}

void CallManager::onCallSignalReceived(const QString& from, const QJsonObject& payload) {
    const QString kind = payload.value("kind").toString();
    if (kind == QStringLiteral("offer")) {
        PeerConnectionEntry* entry = ensurePeerConnection(from);
        if (entry == nullptr) {
            return;
        }
        std::unique_ptr<webrtc::SessionDescriptionInterface> description =
            webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, payload.value("sdp").toString().toStdString());
        if (!description) {
            emit callError(QStringLiteral("Malformed offer from %1").arg(from));
            return;
        }
        const webrtc::scoped_refptr<RemoteDescriptionSetObserver> observer =
            webrtc::make_ref_counted<RemoteDescriptionSetObserver>(*this, from);
        entry->connection->SetRemoteDescription(std::move(description), observer);
    } else if (kind == QStringLiteral("answer")) {
        const auto it = peers_.find(from.toStdString());
        if (it == peers_.end() || !it->second.connection) {
            return;
        }
        std::unique_ptr<webrtc::SessionDescriptionInterface> description = webrtc::CreateSessionDescription(
            webrtc::SdpType::kAnswer, payload.value("sdp").toString().toStdString());
        if (!description) {
            emit callError(QStringLiteral("Malformed answer from %1").arg(from));
            return;
        }
        const webrtc::scoped_refptr<RemoteDescriptionSetObserver> observer =
            webrtc::make_ref_counted<RemoteDescriptionSetObserver>(*this, from);
        it->second.connection->SetRemoteDescription(std::move(description), observer);
    } else if (kind == QStringLiteral("ice")) {
        const auto it = peers_.find(from.toStdString());
        if (it == peers_.end() || !it->second.connection) {
            return;
        }
        webrtc::SdpParseError parseError;
        const std::unique_ptr<webrtc::IceCandidate> candidate = webrtc::IceCandidate::Create(
            payload.value("sdpMid").toString().toStdString(), payload.value("sdpMLineIndex").toInt(),
            payload.value("candidate").toString().toStdString(), &parseError);
        if (!candidate) {
            emit callError(QStringLiteral("Malformed ICE candidate from %1: %2")
                               .arg(from, QString::fromStdString(parseError.description)));
            return;
        }
        if (!it->second.connection->AddIceCandidate(candidate.get())) {
            emit callError(QStringLiteral("Failed to add ICE candidate from %1").arg(from));
        }
    }
}

}  // namespace devicehub
