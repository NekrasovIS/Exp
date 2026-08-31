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
#include <api/video/video_frame.h>
#include <api/video/video_frame_buffer.h>
#include <api/video/video_sink_interface.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <libyuv/convert_argb.h>

#include <QAudioFormat>
#include <QImage>
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

    // OnRenegotiationNeeded() is deliberately left at its inherited
    // no-op: CallManager always knows exactly when it changed a
    // connection's tracks (it's the one calling AddTrack()), so it
    // negotiates explicitly right there instead — see enableVideo().
    // Reacting to this notification too raced with that explicit call
    // for the very first AddTrack() in ensurePeerConnection() (both
    // land on negotiateLocal() for the same peer moments apart),
    // stacking a second offer on top of the first and corrupting the
    // exchange — caught by live testing, not just theorized.

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

    // Fires when a remote track (audio or video) is added to this
    // connection — issue #91 only cares about video, handleRemoteTrack()
    // filters for it. Same GUI-thread hop as every other callback here.
    void OnTrack(webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override {
        CallManager* manager = &manager_;
        const QString peerLogin = peerLogin_;
        QMetaObject::invokeMethod(
            manager, [manager, peerLogin, transceiver] { manager->handleRemoteTrack(peerLogin, transceiver); },
            Qt::QueuedConnection);
    }

private:
    CallManager& manager_;
    QString peerLogin_;
};

/// webrtc::VideoSinkInterface adapter for a remote peer's incoming video
/// track (issue #91) — the receive-side counterpart to
/// CallVideoTrackSource's send-side ARGBToI420 conversion. OnFrame()
/// fires on a WebRTC decode/render thread, not the GUI thread, so —
/// same as PeerObserver above — it hops back via
/// QMetaObject::invokeMethod before touching CallManager.
class CallManager::RemoteVideoSink : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    RemoteVideoSink(CallManager& manager, QString peerLogin) : manager_(manager), peerLogin_(std::move(peerLogin)) {}

    void OnFrame(const webrtc::VideoFrame& frame) override {
        const webrtc::scoped_refptr<webrtc::I420BufferInterface> i420 = frame.video_frame_buffer()->ToI420();
        QImage image(i420->width(), i420->height(), QImage::Format_ARGB32);
        libyuv::I420ToARGB(i420->DataY(), i420->StrideY(), i420->DataU(), i420->StrideU(), i420->DataV(),
                            i420->StrideV(), image.bits(), static_cast<int>(image.bytesPerLine()), i420->width(),
                            i420->height());
        CallManager* manager = &manager_;
        const QString peerLogin = peerLogin_;
        QMetaObject::invokeMethod(
            manager, [manager, peerLogin, image] { manager->handleRemoteVideoFrame(peerLogin, image); },
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

CallManager::CallManager(ChatClient& chatClient, AudioInputDevice& audioInput, AudioOutputDevice& audioOutput,
                          CameraDevice& camera, ScreenCaptureDevice& screenCapture, QObject* parent)
    : QObject(parent),
      chatClient_(chatClient),
      audioInput_(audioInput),
      audioOutput_(audioOutput),
      camera_(camera),
      screenCapture_(screenCapture) {
    connect(&chatClient_, &ChatClient::callRosterReceived, this, &CallManager::onCallRosterReceived);
    connect(&chatClient_, &ChatClient::callPeerJoined, this, &CallManager::onCallPeerJoined);
    connect(&chatClient_, &ChatClient::callPeerLeft, this, &CallManager::onCallPeerLeft);
    connect(&chatClient_, &ChatClient::callSignalReceived, this, &CallManager::onCallSignalReceived);
    connect(&audioInput_, &AudioInputDevice::pcmDataAvailable, this, &CallManager::onCapturedPcm);
    connect(&camera_, &CameraDevice::frameAvailable, this, &CallManager::onCameraFrame);
    connect(&screenCapture_, &ScreenCaptureDevice::frameAvailable, this, &CallManager::onScreenShareFrame);
}

CallManager::~CallManager() {
    leaveCall();
}

void CallManager::joinCall(const QAudioDevice& inputDevice, const QAudioDevice& outputDevice) {
    if (inCall_) {
        return;
    }
    ensureFactory();

    // A null device (e.g. nothing selected/enumerated) has an all-zero
    // preferredFormat() — feeding a 0 channel count into WebRTC's audio
    // pipeline is a hard crash there (fatal CHECK), not a graceful
    // failure, so this has to be caught here first. The call still
    // proceeds without local audio playout — mesh signaling doesn't
    // depend on it.
    if (outputDevice.isNull()) {
        emit callError(tr("No audio output device selected — call will be silent"));
    } else {
        QAudioFormat outputFormat = outputDevice.preferredFormat();
        outputFormat.setSampleFormat(QAudioFormat::Int16);
        audioDeviceModule_->setPlayoutFormat(outputFormat.sampleRate(),
                                              static_cast<size_t>(outputFormat.channelCount()));
        audioOutput_.startStreaming(outputDevice, outputFormat);
    }

    if (inputDevice.isNull()) {
        emit callError(tr("No microphone selected — nothing will be sent"));
    } else {
        audioInput_.start(inputDevice);
    }

    inCall_ = true;
    chatClient_.joinCall();
}

void CallManager::leaveCall() {
    if (!inCall_) {
        return;
    }
    inCall_ = false;
    chatClient_.leaveCall();
    audioInput_.stop();
    audioOutput_.stop();
    for (auto& [login, entry] : peers_) {
        if (entry.connection) {
            entry.connection->Close();
        }
    }
    peers_.clear();
}

void CallManager::setMuted(bool muted) {
    muted_ = muted;
    if (localAudioTrack_) {
        localAudioTrack_->set_enabled(!muted_);
    }
}

void CallManager::ensureLocalVideoTrack() {
    ensureFactory();
    if (localVideoTrack_) {
        return;
    }
    videoTrackSource_ = webrtc::make_ref_counted<CallVideoTrackSource>(/*isScreencast=*/false);
    localVideoTrack_ = peerConnectionFactory_->CreateVideoTrack(videoTrackSource_, "call-video0");
    // First-ever enableVideo()/enableScreenShare() call: attach the
    // (new) track to every peer connection that already exists, and
    // negotiate that change explicitly right here (the same pattern
    // ensurePeerConnection()/onCallRosterReceived() already use for the
    // initial audio track — see the class doc comment for why this
    // stays explicit rather than reacting to WebRTC's own
    // OnRenegotiationNeeded() notification). Any connection created
    // after this point picks up the track as part of its own initial
    // offer/answer instead (see ensurePeerConnection()).
    //
    // Later toggles just flip set_enabled() below, deliberately never
    // removing the track again — RemoveTrackOrError() hit a real fatal
    // assertion inside WebRTC's own codec-list handling during live
    // testing (media/base/codec_list.cc, "Check failed: present_codec
    // == codec"). set_enabled(false) achieves the same practical effect
    // (no video sent) via the exact mechanism setMuted() already uses
    // for audio, without touching tracks (and thus needing
    // renegotiation) at all.
    for (auto& [login, entry] : peers_) {
        attachVideoTrack(entry);
        negotiateLocal(QString::fromStdString(login));
    }
}

void CallManager::enableVideo(const QCameraDevice& device) {
    if (screenShareEnabled_) {
        disableScreenShare();
    }
    ensureLocalVideoTrack();
    videoTrackSource_->setIsScreencast(false);
    localVideoTrack_->set_enabled(true);
    videoEnabled_ = true;
    camera_.setDevice(device);
    camera_.start();
}

void CallManager::disableVideo() {
    if (!videoEnabled_) {
        return;
    }
    videoEnabled_ = false;
    camera_.stop();
    if (localVideoTrack_) {
        localVideoTrack_->set_enabled(false);
    }
}

void CallManager::enableScreenShare(QScreen* screen) {
    if (videoEnabled_) {
        disableVideo();
    }
    ensureLocalVideoTrack();
    videoTrackSource_->setIsScreencast(true);
    localVideoTrack_->set_enabled(true);
    screenShareEnabled_ = true;
    screenCapture_.setScreen(screen);
    screenCapture_.start();
}

void CallManager::disableScreenShare() {
    if (!screenShareEnabled_) {
        return;
    }
    screenShareEnabled_ = false;
    screenCapture_.stop();
    if (localVideoTrack_) {
        localVideoTrack_->set_enabled(false);
    }
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

    // Fires on the ADM's own playout thread (never the Qt GUI thread) —
    // hop back via invokeMethod before touching audioOutput_, same
    // pattern as the PeerConnection observer callbacks below.
    audioDeviceModule_ = webrtc::make_ref_counted<CallAudioDeviceModule>(
        [this](const int16_t* samples, size_t frameCount, int /*sampleRateHz*/, size_t channels) {
            QByteArray pcm(reinterpret_cast<const char*>(samples),
                           static_cast<qsizetype>(frameCount * channels * sizeof(int16_t)));
            QMetaObject::invokeMethod(
                this, [this, pcm] { audioOutput_.writeAudio(pcm); }, Qt::QueuedConnection);
        });

    peerConnectionFactory_ = webrtc::CreatePeerConnectionFactory(
        networkThread_.get(), workerThread_.get(), signalingThread_.get(), audioDeviceModule_,
        webrtc::CreateBuiltinAudioEncoderFactory(), webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(), webrtc::CreateBuiltinVideoDecoderFactory(),
        /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

    // WebRTC's echo canceller needs an accurate render-signal delay
    // estimate to time-align what it's currently playing out against
    // what the mic just captured — feeding it 0 (this class used to)
    // is worse than disabling it outright when there's real playout
    // buffering, which there is (AudioOutputDevice's streaming path).
    // Left at APM defaults (enabled) now that the delay is reported.
    audioDeviceModule_->setTotalDelayMs(AudioOutputDevice::streamingBufferDurationMs());

    const webrtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource =
        peerConnectionFactory_->CreateAudioSource(webrtc::AudioOptions());
    localAudioTrack_ = peerConnectionFactory_->CreateAudioTrack("call-audio0", audioSource.get());
    localAudioTrack_->set_enabled(!muted_);
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

    PeerConnectionEntry& insertedEntry = peers_.emplace(key, std::move(entry)).first->second;
    // Part of this connection's initial offer/answer, not a separate
    // renegotiation — if the video track already exists (even
    // currently disabled), a new peer's connection includes it from
    // the start.
    attachVideoTrack(insertedEntry);
    return &insertedEntry;
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
    // Only stable (nothing pending -> creates an offer) or
    // have-remote-offer (we just received one -> creates an answer) are
    // safe states to (re)negotiate from. Skip otherwise — in
    // particular, have-local-offer means an offer from this function is
    // already in flight (e.g. AddTrack() firing PeerObserver's
    // OnRenegotiationNeeded() while an explicit negotiateLocal() call
    // for the same change is already underway, as happens for the very
    // first AddTrack() in ensurePeerConnection()) — calling
    // SetLocalDescription() again on top of it stacks a second offer
    // and corrupts the exchange, which real live testing caught.
    const webrtc::PeerConnectionInterface::SignalingState state = it->second.connection->signaling_state();
    if (state != webrtc::PeerConnectionInterface::kStable &&
        state != webrtc::PeerConnectionInterface::kHaveRemoteOffer) {
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

void CallManager::handleRemoteTrack(const QString& peerLogin,
                                     webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end() || !transceiver || !transceiver->receiver() || it->second.remoteVideoSink) {
        return;
    }
    const webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = transceiver->receiver()->track();
    if (!track || track->kind() != webrtc::MediaStreamTrackInterface::kVideoKind) {
        return;
    }
    it->second.remoteVideoSink = std::make_unique<RemoteVideoSink>(*this, peerLogin);
    static_cast<webrtc::VideoTrackInterface*>(track.get())
        ->AddOrUpdateSink(it->second.remoteVideoSink.get(), webrtc::VideoSinkWants());
}

void CallManager::handleRemoteVideoFrame(const QString& peerLogin, const QImage& frame) {
    emit remoteVideoFrameReceived(peerLogin, frame);
}

void CallManager::onCapturedPcm(const QByteArray& data, const QAudioFormat& format) {
    if (!audioDeviceModule_ || !inCall_) {
        return;
    }
    const auto channels = static_cast<size_t>(format.channelCount());
    if (channels == 0) {
        return;
    }
    const auto* samples = reinterpret_cast<const int16_t*>(data.constData());
    const size_t frameCount = static_cast<size_t>(data.size()) / sizeof(int16_t) / channels;
    audioDeviceModule_->pushCapturedAudio(samples, frameCount, format.sampleRate(), channels);
}

void CallManager::onCameraFrame(const QVideoFrame& frame) {
    if (!videoEnabled_ || !videoTrackSource_) {
        return;
    }
    videoTrackSource_->pushFrame(frame);
}

void CallManager::onScreenShareFrame(const QVideoFrame& frame) {
    if (!screenShareEnabled_ || !videoTrackSource_) {
        return;
    }
    videoTrackSource_->pushFrame(frame);
}

void CallManager::attachVideoTrack(PeerConnectionEntry& entry) {
    if (!localVideoTrack_ || entry.videoSender || !entry.connection) {
        return;
    }
    const webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> addTrackResult =
        entry.connection->AddTrack(localVideoTrack_, std::vector<std::string>{"call-stream"});
    if (!addTrackResult.ok()) {
        emit callError(
            QStringLiteral("Failed to attach video: %1").arg(QString::fromUtf8(addTrackResult.error().message())));
        return;
    }
    entry.videoSender = addTrackResult.value();
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
    emit remoteVideoTrackRemoved(login);
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
