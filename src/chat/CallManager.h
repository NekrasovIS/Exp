#pragma once

#include "chat/ChatClient.h"
#include "devices/CallAudioDeviceModule.h"

#include <api/peer_connection_interface.h>
#include <api/scoped_refptr.h>
#include <rtc_base/thread.h>

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <memory>
#include <string>
#include <unordered_map>

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
 * First-pass scope: mesh signaling only. The CallAudioDeviceModule's
 * capture side is never fed real microphone PCM here (pushCapturedAudio()
 * is not called), and its playout sink is left unset — wiring real
 * AudioInputDevice/AudioOutputDevice audio through is deferred to a
 * follow-up (issue #46 Phase 4 UI or a dedicated task), so peers connect
 * and negotiate correctly but carry silence until that lands.
 */
class CallManager : public QObject {
    Q_OBJECT

public:
    explicit CallManager(ChatClient& chatClient, QObject* parent = nullptr);
    ~CallManager() override;

    /// Sends call_join and starts offering to whoever's already in the
    /// call, once the roster reply arrives.
    void joinCall();

    /// Sends call_leave and tears down every peer connection.
    void leaveCall();

    [[nodiscard]] bool inCall() const { return inCall_; }

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

    ChatClient& chatClient_;

    std::unique_ptr<webrtc::Thread> networkThread_;
    std::unique_ptr<webrtc::Thread> workerThread_;
    std::unique_ptr<webrtc::Thread> signalingThread_;
    webrtc::scoped_refptr<CallAudioDeviceModule> audioDeviceModule_;
    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory_;
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> localAudioTrack_;

    std::unordered_map<std::string, PeerConnectionEntry> peers_;
    bool inCall_ = false;
};

}  // namespace devicehub
