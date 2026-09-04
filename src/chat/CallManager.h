#pragma once

#include "chat/ChatClient.h"
#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CallAudioDeviceModule.h"
#include "devices/CallVideoTrackSource.h"
#include "devices/CameraDevice.h"
#include "devices/ScreenCaptureDevice.h"

#include <api/peer_connection_interface.h>
#include <api/rtp_transceiver_interface.h>
#include <api/scoped_refptr.h>
#include <rtc_base/thread.h>

#include <QAudioDevice>
#include <QByteArray>
#include <QCameraDevice>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVideoFrame>

#include <memory>
#include <string>
#include <unordered_map>

class QAudioFormat;
class QScreen;

namespace devicehub {

/**
 * @brief Оркестрация mesh-голосовых звонков: один `webrtc::PeerConnection`
 *        на каждого удалённого участника звонка, подключённый к релею
 *        сигналинга звонков в ChatClient (issue #46, Phase 3).
 *
 * Владеет единственным `webrtc::PeerConnectionFactoryInterface`
 * (создаётся лениво, при первом joinCall()) и `CallAudioDeviceModule`
 * (issue #64), зарегистрированным как её аудиоустройство — каждый
 * PeerConnection, создаваемый этим классом, использует общую
 * фабрику/ADM, поэтому входящее аудио от всех пиров микшируется в один
 * поток воспроизведения самим libwebrtc.
 *
 * Правило инициации mesh (избегает одновременных офферов в одной паре):
 * только что присоединившийся пир всегда отправляет offer каждому
 * участнику, уже присутствующему в ростере; уже присутствующие участники
 * только отвечают. Этот класс следует этому правилу, отправляя offer
 * всем в списке из callRosterReceived() и отвечая только в ответ на
 * входящий offer (callSignalReceived()) — никогда в callPeerJoined(),
 * который носит чисто информационный характер.
 *
 * PeerConnectionObserver и колбэки observer'ов SetLocal/RemoteDescription
 * срабатывают на собственном signaling-потоке WebRTC, никогда на
 * GUI-потоке Qt — каждый из них немедленно перепрыгивает обратно через
 * QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection) прежде чем
 * трогать `peers_` или вызывать что-либо в ChatClient, так что всё
 * собственное состояние этого класса трогается только из того
 * GUI-потока, в котором он живёт.
 *
 * Настоящее аудио с микрофона/динамиков (issue #70): pcmDataAvailable()
 * у AudioInputDevice перенаправляется в pushCapturedAudio() у ADM, а
 * playout-sink у ADM пишет в потоковый путь AudioOutputDevice — обе
 * настраиваются в joinCall(), разбираются в leaveCall(). Оба устройства
 * поддерживают только одного владельца одновременно (см. их собственные
 * doc-комментарии): звонок и элементы управления mic-test/test-tone в
 * диалоге настроек делят одни и те же экземпляры
 * AudioInputDevice/AudioOutputDevice, поэтому запуск одного, пока
 * активен другой, молча перехватывает нижележащий
 * QAudioSource/QAudioSink. Это принятый на первом этапе компромисс, не
 * решённый здесь — соответствует уже существующей семантике устройств в
 * диалоге настроек.
 *
 * Видео с камеры (issue #72) — опционально и отделено от joinCall().
 * Общий видеотрек создаётся один раз, при самом первом вызове
 * enableVideo(), и подключается ко всем PeerConnection, существующим на
 * этот момент — единственный случай, добавляющий трек к уже
 * согласованному соединению, поэтому enableVideo() явно вызывает
 * negotiateLocal() для каждого из них тут же на месте (так же, как это
 * уже делают ensurePeerConnection()/onCallRosterReceived() для
 * изначального аудиотрека, вместо того чтобы реагировать на собственное
 * уведомление WebRTC OnRenegotiationNeeded() — почему, см. doc-комментарий
 * к этому методу у PeerObserver: опора одновременно и на явный вызов, и
 * на это уведомление для одного и того же изменения трека приводила к
 * гонке между ними при живом тестировании, повреждая обмен). Более
 * поздний PeerConnection (ensurePeerConnection()) подхватывает трек уже
 * как часть своего собственного изначального offer/answer. После этого
 * первого подключения enableVideo()/disableVideo() лишь переключают
 * VideoTrackInterface::set_enabled() — тот же механизм, что setMuted()
 * использует для аудио — и намеренно никогда больше не удаляют трек:
 * RemoveTrackOrError() приводил к реальному фатальному assert внутри
 * обработки списка кодеков самого WebRTC при живом тестировании более
 * ранней версии этого класса (media/base/codec_list.cc, "Check failed:
 * present_codec == codec"), поэтому этот класс полностью избегает этого
 * пути, вместо того чтобы полагаться на исправление внутреннего бага
 * WebRTC.
 *
 * Демонстрация экрана (issue #112) поначалу переиспользовала один общий
 * видеотрек с камерой (взаимно исключая их), но issue #185 разделил это
 * на два полностью независимых трека/источника — cameraTrackSource_/
 * localCameraTrack_ и screenShareTrackSource_/localScreenShareTrack_,
 * каждый ensure*() (ensureLocalCameraTrack()/ensureLocalScreenShareTrack())
 * создаётся один раз и подключается ко всем текущим пирам тем же
 * паттерном «создать → attachXxxTrack() → явный negotiateLocal() для
 * каждого», что раньше был общим для обоих; attachTrack() — маленький
 * общий хелпер под обоими attachCameraTrack()/attachScreenShareTrack(),
 * чтобы не дублировать AddTrack()+обработку ошибки. enableVideo() и
 * enableScreenShare() теперь можно включать одновременно — реального
 * ограничения от WebRTC тут никогда не было, только цена второго трека
 * (лишний renegotiation/подключение на пира при первом использовании
 * каждого), которую issue #185 решил заплатить.
 *
 * Приём удалённого видео (issue #91) — зеркальное отражение его
 * отправки, тоже раздвоенное на камеру/экран (issue #185):
 * PeerObserver::OnTrack() срабатывает, как только появляется
 * видео-transceiver пира, handleRemoteTrack() различает, какой из двух
 * это треков, по его id (kCameraTrackId/kScreenShareTrackId — те же
 * строки, что заданы при CreateVideoTrack() на отправляющей стороне и
 * доходят до приёмника через msid в SDP), и подключает RemoteVideoSink
 * к соответствующему полю PeerConnectionEntry::remoteCameraVideoSink/
 * remoteScreenShareVideoSink (по отдельности защищает каждое от
 * повторного подключения). RemoteVideoSink знает, какого он рода, и
 * передаёт это дальше вместе с кадром — remoteVideoFrameReceived()
 * получила параметр isScreenShare, чтобы UI мог показать камеру и
 * демонстрацию экрана одного и того же участника как две разные
 * плитки, а не путать их в одну. Конвертация кадра (I420 -> QImage,
 * обратная ARGBToI420 из CallVideoTrackSource) и переход на GUI-поток
 * (OnFrame() срабатывает на потоке декодирования WebRTC) не изменились.
 */
class CallManager : public QObject {
    Q_OBJECT

public:
    CallManager(ChatClient& chatClient, AudioInputDevice& audioInput, AudioOutputDevice& audioOutput,
                CameraDevice& camera, ScreenCaptureDevice& screenCapture, QObject* parent = nullptr);
    ~CallManager() override;

    /// Отправляет call_join, начинает захват с @p inputDevice и
    /// стриминг удалённого аудио на @p outputDevice, и начинает
    /// отправлять offer всем, кто уже в звонке, как только придёт ответ
    /// с ростером.
    void joinCall(const QAudioDevice& inputDevice, const QAudioDevice& outputDevice);

    /// Отправляет call_leave и разбирает все соединения с пирами.
    void leaveCall();

    [[nodiscard]] bool inCall() const { return inCall_; }

    /// Заглушает/включает локальный аудиотрек сразу для всех пиров —
    /// настоящий mute на уровне трека
    /// (webrtc::AudioTrackInterface::set_enabled), а не переключатель
    /// только в UI. Безопасно вызывать независимо от того, активен ли
    /// звонок; применяется к localAudioTrack_, если/когда он существует.
    void setMuted(bool muted);

    [[nodiscard]] bool isMuted() const { return muted_; }

    /// Начинает захват с @p device и отправку его как видеотрека
    /// каждому текущему и будущему пиру — добавляется к существующим
    /// PeerConnection через renegotiation, к новым — как часть их
    /// изначального offer/answer. Независимо от enableScreenShare()
    /// (issue #185 — отдельный трек, а не общий) и от того, активен ли
    /// звонок — оба безопасно вызывать в любом порядке и сочетании.
    void enableVideo(const QCameraDevice& device);

    /// Останавливает камеру и отключает видеотрек камеры у всех пиров —
    /// демонстрация экрана, если она тоже активна, не затрагивается.
    void disableVideo();

    [[nodiscard]] bool videoEnabled() const { return videoEnabled_; }

    /// Начинает захват @p screen и отправку его как отдельного
    /// локального видеотрека (issue #112, независимый трек с issue
    /// #185) — не взаимоисключает видео с камеры, оба можно включить
    /// одновременно. Безопасно вызывать независимо от того, активен ли
    /// звонок.
    void enableScreenShare(QScreen* screen);

    /// Останавливает захват экрана и отключает видеотрек демонстрации
    /// экрана у всех пиров — видео с камеры, если оно тоже активно, не
    /// затрагивается.
    void disableScreenShare();

    [[nodiscard]] bool screenShareEnabled() const { return screenShareEnabled_; }

signals:
    void participantJoined(const QString& login);
    void participantLeft(const QString& login);
    void callError(const QString& message);

    /// Декодированный кадр из входящего видеотрека удалённого участника
    /// (issue #91) — никогда не испускается для пира, не отправлявшего
    /// видео. @p isScreenShare различает камеру и демонстрацию экрана
    /// одного и того же участника (issue #185, независимые треки) — UI
    /// показывает их как отдельные плитки.
    void remoteVideoFrameReceived(const QString& peerLogin, const QImage& frame, bool isScreenShare);
    /// Видеотрек данного рода (камера/демонстрация экрана, см.
    /// @p isScreenShare) у этого пира пропал — UI должен убрать
    /// соответствующую видео-плитку. Испускается для обоих родов сразу
    /// при уходе участника из звонка, даже если он в реальности слал
    /// только один — UI-сторона просто ничего не найдёт для отсутствующего
    /// и промолчит.
    void remoteVideoTrackRemoved(const QString& peerLogin, bool isScreenShare);

private:
    class PeerObserver;
    class RemoteVideoSink;
    class LocalDescriptionSetObserver;
    class RemoteDescriptionSetObserver;
    friend class PeerObserver;
    friend class RemoteVideoSink;
    friend class LocalDescriptionSetObserver;
    friend class RemoteDescriptionSetObserver;

    struct PeerConnectionEntry {
        webrtc::scoped_refptr<webrtc::PeerConnectionInterface> connection;
        std::unique_ptr<PeerObserver> observer;
        /// Не null, как только localCameraTrack_/localScreenShareTrack_
        /// подключён к этому пиру — защищает attachCameraTrack()/
        /// attachScreenShareTrack() от повторного добавления. Трек,
        /// будучи подключённым, больше никогда не удаляется (см.
        /// doc-комментарий enableVideo()), поэтому обратно в null эти
        /// поля не возвращаются.
        webrtc::scoped_refptr<webrtc::RtpSenderInterface> cameraSender;
        webrtc::scoped_refptr<webrtc::RtpSenderInterface> screenShareSender;
        /// Не null, как только для этого пира замечен соответствующий
        /// удалённый видеотрек (PeerObserver::OnTrack() ->
        /// handleRemoteTrack(), различает камеру/экран по id трека) —
        /// защищает каждый от повторного подключения приёмного sink'а
        /// независимо от другого (issue #185 — камера и демонстрация
        /// экрана участника могут идти одновременно, каждая своим
        /// треком).
        std::unique_ptr<RemoteVideoSink> remoteCameraVideoSink;
        std::unique_ptr<RemoteVideoSink> remoteScreenShareVideoSink;
    };

    void onCallRosterReceived(const QStringList& participants);
    void onCallPeerJoined(const QString& login);
    void onCallPeerLeft(const QString& login);
    void onCallSignalReceived(const QString& from, const QJsonObject& payload);

    void ensureFactory();
    PeerConnectionEntry* ensurePeerConnection(const QString& peerLogin);
    void closePeerConnection(const QString& peerLogin);

    /// Запускает (пере)согласование local-description для
    /// PeerConnection пира `peerLogin` — создаёт и устанавливает offer
    /// или answer, в зависимости от того, чего требует текущее
    /// signaling-состояние соединения.
    void negotiateLocal(const QString& peerLogin);

    void handleLocalDescriptionSet(const QString& peerLogin, bool ok, const QString& errorMessage);
    void handleRemoteDescriptionSet(const QString& peerLogin, bool ok, const QString& errorMessage);
    void handleLocalIceCandidate(const QString& peerLogin, const QJsonObject& payload);

    /// На соединении пира `peerLogin` появился новый (или
    /// пересогласованный) transceiver — если он несёт видеотрек и у
    /// этого пира ещё нет приёмного sink'а нужного рода (камера/экран,
    /// определяется по id трека — kCameraTrackId/kScreenShareTrackId),
    /// подключает его, чтобы кадры начали поступать в
    /// remoteVideoFrameReceived().
    void handleRemoteTrack(const QString& peerLogin, webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);
    void handleRemoteVideoFrame(const QString& peerLogin, const QImage& frame, bool isScreenShare);

    /// Перенаправляет захваченный буфер из audioInput_ в ADM, пока
    /// звонок активен.
    void onCapturedPcm(const QByteArray& data, const QAudioFormat& format);

    /// Перенаправляет захваченный кадр из camera_ в cameraTrackSource_,
    /// пока видео включено.
    void onCameraFrame(const QVideoFrame& frame);

    /// Перенаправляет захваченный кадр из screenCapture_ в
    /// screenShareTrackSource_, пока активна демонстрация экрана.
    void onScreenShareFrame(const QVideoFrame& frame);

    /// Создаёт cameraTrackSource_/localCameraTrack_ и подключает их ко
    /// всем существующим пирам, если это первый вызов enableVideo() за
    /// всё время — иначе ничего не делает (см. doc-комментарий
    /// enableVideo() о том, почему трек, будучи создан, больше никогда
    /// не удаляется).
    void ensureLocalCameraTrack();

    /// То же самое, что ensureLocalCameraTrack(), для отдельного трека
    /// демонстрации экрана (issue #185).
    void ensureLocalScreenShareTrack();

    /// Добавляет localCameraTrack_ к соединению `entry`, если оно
    /// существует и ещё не подключено — независимо от того, включено
    /// ли видео сейчас (см. doc-комментарий enableVideo() о том, почему
    /// трек, будучи создан, больше никогда не удаляется, а только
    /// переключается через set_enabled()).
    void attachCameraTrack(PeerConnectionEntry& entry);

    /// То же самое, что attachCameraTrack(), для localScreenShareTrack_
    /// (issue #185).
    void attachScreenShareTrack(PeerConnectionEntry& entry);

    /// Общая часть attachCameraTrack()/attachScreenShareTrack() —
    /// AddTrack() @p track к `entry` в @p sender, если он ещё не был
    /// добавлен, с одинаковой обработкой ошибки в обоих случаях.
    void attachTrack(PeerConnectionEntry& entry, const webrtc::scoped_refptr<webrtc::VideoTrackInterface>& track,
                      webrtc::scoped_refptr<webrtc::RtpSenderInterface>& sender);

    ChatClient& chatClient_;
    AudioInputDevice& audioInput_;
    AudioOutputDevice& audioOutput_;
    CameraDevice& camera_;
    ScreenCaptureDevice& screenCapture_;

    std::unique_ptr<webrtc::Thread> networkThread_;
    std::unique_ptr<webrtc::Thread> workerThread_;
    std::unique_ptr<webrtc::Thread> signalingThread_;
    webrtc::scoped_refptr<CallAudioDeviceModule> audioDeviceModule_;
    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory_;
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> localAudioTrack_;
    webrtc::scoped_refptr<CallVideoTrackSource> cameraTrackSource_;
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> localCameraTrack_;
    webrtc::scoped_refptr<CallVideoTrackSource> screenShareTrackSource_;
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> localScreenShareTrack_;

    std::unordered_map<std::string, PeerConnectionEntry> peers_;
    bool inCall_ = false;
    bool muted_ = false;
    bool videoEnabled_ = false;
    bool screenShareEnabled_ = false;
};

}  // namespace devicehub
