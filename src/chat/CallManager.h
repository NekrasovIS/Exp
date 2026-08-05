#pragma once

#include "chat/ChatClient.h"
#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CallAudioDeviceModule.h"
#include "devices/CallVideoTrackSource.h"
#include "devices/CameraDevice.h"

#include <api/peer_connection_interface.h>
#include <api/scoped_refptr.h>
#include <rtc_base/thread.h>

#include <QAudioDevice>
#include <QByteArray>
#include <QCameraDevice>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVideoFrame>

#include <memory>
#include <string>
#include <unordered_map>

class QAudioFormat;

namespace devicehub {

/**
 * @brief Mesh voice-call orchestration: one `webrtc::PeerConnection` per
 *        remote call participant, wired to ChatClient's call-signaling
 *        relay (issue #46, Phase 3).
 *
 * Owns the one `webrtc::PeerConnectionFactoryInterface` (built lazily, on
 * first joinCall()) and a `CallAudioDeviceModule` (issue #64) registered
 * as its audio device — every PeerConnection this class creates shares
 * that same factory/ADM, so incoming audio from every peer is mixed into
 * one playout stream by libwebrtc itself.
 *
 * Mesh initiation rule (avoids simultaneous offers in one pairing): the
 * newly-joining peer always sends the offer to each participant already
 * in the roster; existing participants only ever answer. This class
 * follows that rule by offering to everyone in callRosterReceived()'s
 * list, and only ever answering in response to an incoming offer
 * (callSignalReceived()) — never on callPeerJoined(), which is purely
 * informational.
 *
 * PeerConnectionObserver and the SetLocal/RemoteDescription observer
 * callbacks fire on WebRTC's own signaling thread, never the Qt GUI
 * thread — every one of them immediately hops back via
 * QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection) before
 * touching `peers_` or calling into ChatClient, so all of this class's
 * own state is only ever touched from the GUI thread it lives on.
 *
 * Real microphone/speaker audio (issue #70): AudioInputDevice's
 * pcmDataAvailable() is forwarded into the ADM's pushCapturedAudio(),
 * and the ADM's playout sink writes into AudioOutputDevice's streaming
 * path — both set up in joinCall(), torn down in leaveCall(). Both
 * devices are single-owner-at-a-time (see their own doc comments): a
 * call and the Settings dialog's mic-test/test-tone controls share the
 * same AudioInputDevice/AudioOutputDevice instances, so starting one
 * while the other is active silently takes over the underlying
 * QAudioSource/QAudioSink. Accepted first-pass tradeoff, not solved
 * here — matches the existing settings-dialog device semantics.
 *
 * Camera video (issue #72) is opt-in and separate from joinCall(). The
 * shared video track is created once, on the first ever enableVideo()
 * call, and attached to every PeerConnection that exists at that point
 * — the one case that adds a track to an already-negotiated
 * connection, so enableVideo() calls negotiateLocal() for each of them
 * right there, explicitly (same as ensurePeerConnection()/
 * onCallRosterReceived() already do for the initial audio track,
 * rather than reacting to WebRTC's own OnRenegotiationNeeded()
 * notification — see PeerObserver's doc comment on that method for
 * why: relying on both an explicit call and that notification for the
 * same track change raced the two against each other during live
 * testing, corrupting the exchange). A PeerConnection created later
 * (ensurePeerConnection()) picks up the track as part of its own
 * initial offer/answer instead. After that first attach,
 * enableVideo()/disableVideo() only ever toggle
 * VideoTrackInterface::set_enabled() — same mechanism setMuted() uses
 * for audio — and deliberately never remove the track again:
 * RemoveTrackOrError() hit a real fatal assertion inside WebRTC's own
 * codec-list handling during live testing of an earlier version of
 * this class (media/base/codec_list.cc, "Check failed: present_codec
 * == codec"), so this class avoids that path entirely rather than
 * relying on an internal WebRTC bug being fixed.
 */
class CallManager : public QObject {
    Q_OBJECT

public:
    CallManager(ChatClient& chatClient, AudioInputDevice& audioInput, AudioOutputDevice& audioOutput,
                CameraDevice& camera, QObject* parent = nullptr);
    ~CallManager() override;

    /// Sends call_join, starts capturing from @p inputDevice and
    /// streaming remote audio to @p outputDevice, and starts offering
    /// to whoever's already in the call once the roster reply arrives.
    void joinCall(const QAudioDevice& inputDevice, const QAudioDevice& outputDevice);

    /// Sends call_leave and tears down every peer connection.
    void leaveCall();

    [[nodiscard]] bool inCall() const { return inCall_; }

    /// Mutes/unmutes the local audio track for every peer at once — a
    /// real per-track mute (webrtc::AudioTrackInterface::set_enabled),
    /// not a UI-only toggle. Safe to call whether or not a call is
    /// active; applied to localAudioTrack_ if/when it exists.
    void setMuted(bool muted);

    [[nodiscard]] bool isMuted() const { return muted_; }

    /// Starts capturing from @p device and sending it as a video track
    /// to every current and future peer — added to existing
    /// PeerConnections via renegotiation, to new ones as part of their
    /// initial offer/answer. Safe to call whether or not a call is
    /// active.
    void enableVideo(const QCameraDevice& device);

    /// Stops the camera and removes the video track from every peer.
    void disableVideo();

    [[nodiscard]] bool videoEnabled() const { return videoEnabled_; }

signals:
    void participantJoined(const QString& login);
    void participantLeft(const QString& login);
    void callError(const QString& message);

private:
    class PeerObserver;
    class LocalDescriptionSetObserver;
    class RemoteDescriptionSetObserver;
    friend class PeerObserver;
    friend class LocalDescriptionSetObserver;
    friend class RemoteDescriptionSetObserver;

    struct PeerConnectionEntry {
        webrtc::scoped_refptr<webrtc::PeerConnectionInterface> connection;
        std::unique_ptr<PeerObserver> observer;
        /// Non-null once localVideoTrack_ has been attached to this
        /// peer — guards attachVideoTrack() against adding it twice.
        /// The track is never removed again once attached (see
        /// enableVideo()'s doc comment), so this never goes back to
        /// null.
        webrtc::scoped_refptr<webrtc::RtpSenderInterface> videoSender;
    };

    void onCallRosterReceived(const QStringList& participants);
    void onCallPeerJoined(const QString& login);
    void onCallPeerLeft(const QString& login);
    void onCallSignalReceived(const QString& from, const QJsonObject& payload);

    void ensureFactory();
    PeerConnectionEntry* ensurePeerConnection(const QString& peerLogin);
    void closePeerConnection(const QString& peerLogin);

    /// Kicks off local-description (re)negotiation for `peerLogin`'s
    /// PeerConnection — creates and sets an offer or an answer,
    /// whichever the connection's current signaling state calls for.
    void negotiateLocal(const QString& peerLogin);

    void handleLocalDescriptionSet(const QString& peerLogin, bool ok, const QString& errorMessage);
    void handleRemoteDescriptionSet(const QString& peerLogin, bool ok, const QString& errorMessage);
    void handleLocalIceCandidate(const QString& peerLogin, const QJsonObject& payload);

    /// Forwards a captured buffer from audioInput_ into the ADM, while a
    /// call is active.
    void onCapturedPcm(const QByteArray& data, const QAudioFormat& format);

    /// Forwards a captured frame from camera_ into videoTrackSource_,
    /// while video is enabled.
    void onCameraFrame(const QVideoFrame& frame);

    /// Adds localVideoTrack_ to `entry`'s connection if it exists and
    /// isn't already attached — independent of whether video is
    /// currently enabled (see enableVideo()'s doc comment for why the
    /// track, once created, is never removed again, only toggled via
    /// set_enabled()).
    void attachVideoTrack(PeerConnectionEntry& entry);

    ChatClient& chatClient_;
    AudioInputDevice& audioInput_;
    AudioOutputDevice& audioOutput_;
    CameraDevice& camera_;

    std::unique_ptr<webrtc::Thread> networkThread_;
    std::unique_ptr<webrtc::Thread> workerThread_;
    std::unique_ptr<webrtc::Thread> signalingThread_;
    webrtc::scoped_refptr<CallAudioDeviceModule> audioDeviceModule_;
    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory_;
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> localAudioTrack_;
    webrtc::scoped_refptr<CallVideoTrackSource> videoTrackSource_;
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> localVideoTrack_;

    std::unordered_map<std::string, PeerConnectionEntry> peers_;
    bool inCall_ = false;
    bool muted_ = false;
    bool videoEnabled_ = false;
};

}  // namespace devicehub
