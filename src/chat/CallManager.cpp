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
#include <QJsonArray>
#include <QJsonValue>
#include <QMetaObject>

#include <utility>
#include <vector>

namespace devicehub {

namespace {
/// Идентификаторы локальных видеотреков (issue #185) — заданы при
/// CreateVideoTrack() на отправляющей стороне и доходят до приёмника
/// через msid в SDP, поэтому handleRemoteTrack() может по ним же
/// различить, какой из двух независимых треков пира (камера или
/// демонстрация экрана) только что появился.
constexpr char kCameraTrackId[] = "call-camera-video0";
constexpr char kScreenShareTrackId[] = "call-screenshare-video0";

/// Условная "метка пира" для publishConnection_ (issue #232) — тот же
/// набор generic-хелперов (negotiateLocal()/handleLocalDescriptionSet()/
/// PeerObserver и т.п.), что уже обслуживает subscribe-записи в peers_ по
/// логину чужого участника, обслуживает и единственное publish-соединение
/// по этой зарезервированной строке; реальным логином она стать не может
/// (login не может быть пустым/содержать эти символы на стороне
/// auth-service).
QString publishConnectionLabel() {
    return QStringLiteral("__sfu_publish__");
}
}  // namespace

/// Адаптер webrtc::PeerConnectionObserver — каждый колбэк срабатывает на
/// signaling-потоке WebRTC и немедленно перепрыгивает обратно на
/// собственный (GUI) поток CallManager через QMetaObject::invokeMethod
/// прежде чем трогать какое-либо общее состояние; почему это безопасно
/// даже если CallManager будет разрушен в процессе — см. doc-комментарий
/// класса CallManager.
class CallManager::PeerObserver : public webrtc::PeerConnectionObserver {
public:
    PeerObserver(CallManager& manager, QString peerLogin) : manager_(manager), peerLogin_(std::move(peerLogin)) {}

    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState /*newState*/) override {}
    void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> /*dataChannel*/) override {}

    // SFU-соединения (issue #232/#233 — единственный путь этого класса
    // теперь) не трикклят ICE-кандидаты по одному — у прокси-протокола
    // chat-service нет отдельного запроса Janus "trickle" (не-trickle
    // сигналинг: кандидаты едут внутри самого SDP), поэтому offer/answer
    // им уходит только здесь, после полного сбора, а не сразу после
    // SetLocalDescription — см. doc-комментарий
    // CallManager::handleIceGatheringComplete().
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) override {
        if (newState != webrtc::PeerConnectionInterface::kIceGatheringComplete) {
            return;
        }
        CallManager* manager = &manager_;
        const QString peerLogin = peerLogin_;
        QMetaObject::invokeMethod(
            manager, [manager, peerLogin] { manager->handleIceGatheringComplete(peerLogin); }, Qt::QueuedConnection);
    }

    // OnRenegotiationNeeded() намеренно оставлен унаследованным no-op:
    // CallManager всегда точно знает, когда он изменил треки
    // соединения (это именно он вызывает AddTrack()), поэтому вместо
    // этого он согласовывает явно там, где для этого есть подходящий
    // момент — см. enableVideo()/enableScreenShare() для добавления
    // видео на лету и ветку "joined" в onJanusEvent() для самого первого
    // AddTrack() аудио в ensurePublishConnection(). Реакция ещё и на это
    // уведомление создавала гонку с явным вызовом negotiateLocal() для
    // того же самого изменения трека с разницей в мгновения, накладывая
    // второй offer поверх первого и повреждая обмен — обнаружено живым
    // тестированием ещё в mesh-версии этого класса (issue #46), а не
    // просто предполагалось теоретически; тот же риск остаётся и здесь.

    // SFU-соединения (issue #232/#233 — единственный путь этого класса
    // теперь) не трикклят кандидаты по одному — см. doc-комментарий
    // OnIceGatheringChange() выше: полный набор уходит единым SDP, после
    // завершения сбора. Обязательный override интерфейса
    // webrtc::PeerConnectionObserver, но намеренно ничего не делает.
    void OnIceCandidate(const webrtc::IceCandidate* /*candidate*/) override {}

    // Срабатывает, когда к этому соединению добавляется удалённый трек
    // (аудио или видео) — issue #91 интересует только видео,
    // handleRemoteTrack() фильтрует именно по нему. Тот же переход на
    // GUI-поток, что и у любого другого колбэка здесь.
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

/// Адаптер webrtc::VideoSinkInterface для входящего видеотрека
/// удалённого пира (issue #91) — приёмный аналог отправляющей
/// конвертации ARGBToI420 из CallVideoTrackSource. OnFrame()
/// срабатывает на потоке декодирования/рендеринга WebRTC, а не на
/// GUI-потоке, поэтому — так же, как PeerObserver выше — он
/// перепрыгивает обратно через QMetaObject::invokeMethod прежде чем
/// трогать CallManager.
class CallManager::RemoteVideoSink : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    RemoteVideoSink(CallManager& manager, QString peerLogin, bool isScreenShare)
        : manager_(manager), peerLogin_(std::move(peerLogin)), isScreenShare_(isScreenShare) {}

    void OnFrame(const webrtc::VideoFrame& frame) override {
        const webrtc::scoped_refptr<webrtc::I420BufferInterface> i420 = frame.video_frame_buffer()->ToI420();
        QImage image(i420->width(), i420->height(), QImage::Format_ARGB32);
        libyuv::I420ToARGB(i420->DataY(), i420->StrideY(), i420->DataU(), i420->StrideU(), i420->DataV(),
                            i420->StrideV(), image.bits(), static_cast<int>(image.bytesPerLine()), i420->width(),
                            i420->height());
        CallManager* manager = &manager_;
        const QString peerLogin = peerLogin_;
        const bool isScreenShare = isScreenShare_;
        QMetaObject::invokeMethod(
            manager,
            [manager, peerLogin, image, isScreenShare] {
                manager->handleRemoteVideoFrame(peerLogin, image, isScreenShare);
            },
            Qt::QueuedConnection);
    }

private:
    CallManager& manager_;
    QString peerLogin_;
    bool isScreenShare_;
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
    connect(&chatClient_, &ChatClient::callPeerJoined, this, &CallManager::onCallPeerJoined);
    connect(&chatClient_, &ChatClient::callPeerLeft, this, &CallManager::onCallPeerLeft);
    connect(&chatClient_, &ChatClient::sfuRoomAssigned, this, &CallManager::onSfuRoomAssigned);
    connect(&chatClient_, &ChatClient::janusAttached, this, &CallManager::onJanusAttached);
    connect(&chatClient_, &ChatClient::janusEventReceived, this, &CallManager::onJanusEvent);
    connect(&chatClient_, &ChatClient::janusMessageAck, this, [this](const QJsonObject& response) {
        if (response.value(QStringLiteral("janus")).toString() == QStringLiteral("error")) {
            emit callError(QStringLiteral("SFU signaling error: %1")
                               .arg(response.value(QStringLiteral("error")).toObject().value(QStringLiteral("reason")).toString()));
        }
    });
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

    // У null-устройства (например, ничего не выбрано/не перечислено)
    // preferredFormat() полностью нулевой — передача количества
    // каналов 0 в аудио-конвейер WebRTC там приводит к жёсткому крашу
    // (фатальный CHECK), а не к плавному отказу, поэтому это нужно
    // отловить здесь заранее. Звонок при этом всё равно продолжается
    // без локального воспроизведения аудио — SFU-сигналинг от него не
    // зависит.
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

    // SFU (issue #232) — publishConnection_ и вся очередь/состояние
    // подписки живут только на время одного звонка, симметрично peers_
    // выше.
    if (publishConnection_.connection) {
        publishConnection_.connection->Close();
    }
    publishConnection_ = PeerConnectionEntry{};
    sfuRoom_.clear();
    ownFeedId_.clear();
    pendingAttach_ = PendingJanusAttach::kNone;
    pendingSubscribeFeedId_.clear();
    pendingSubscribePeerLogin_.clear();
    subscribeQueue_.clear();
}

void CallManager::setMuted(bool muted) {
    muted_ = muted;
    if (localAudioTrack_) {
        localAudioTrack_->set_enabled(!muted_);
    }
}

void CallManager::ensureLocalCameraTrack() {
    ensureFactory();
    if (localCameraTrack_) {
        return;
    }
    cameraTrackSource_ = webrtc::make_ref_counted<CallVideoTrackSource>(/*isScreencast=*/false);
    localCameraTrack_ = peerConnectionFactory_->CreateVideoTrack(cameraTrackSource_, kCameraTrackId);
    // Самый первый вызов enableVideo(): подключаем (новый) трек ко всем
    // уже существующим соединениям с пирами и явно согласовываем это
    // изменение прямо здесь (тот же паттерн, что уже использует ветка
    // "joined" в onJanusEvent() для изначального аудиотрека publish-
    // соединения — почему это остаётся явным, а не реакцией на
    // собственное уведомление WebRTC OnRenegotiationNeeded(), см.
    // doc-комментарий класса). Любое соединение, созданное после этого
    // момента, вместо этого подхватывает трек как часть своего
    // собственного изначального offer/answer (см. attachCameraTrack()).
    //
    // Последующие переключения просто дёргают set_enabled() ниже,
    // намеренно никогда больше не удаляя трек — RemoveTrackOrError()
    // приводил к реальному фатальному assert внутри обработки списка
    // кодеков самого WebRTC при живом тестировании
    // (media/base/codec_list.cc, "Check failed: present_codec ==
    // codec"). set_enabled(false) достигает того же практического
    // эффекта (видео не отправляется) через тот же самый механизм,
    // который setMuted() уже использует для аудио, вообще не трогая
    // треки (и, соответственно, не требуя renegotiation).
    //
    // issue #232: подключаем только к publishConnection_ — peers_
    // теперь subscribe-соединения (мы только принимаем по ним чужие
    // потоки), отправлять на них наше собственное видео бессмысленно;
    // publishConnection_ — единственное соединение, несущее наш
    // исходящий трафик.
    if (publishConnection_.connection) {
        attachCameraTrack(publishConnection_);
        negotiateLocal(publishConnectionLabel());
    }
}

void CallManager::ensureLocalScreenShareTrack() {
    ensureFactory();
    if (localScreenShareTrack_) {
        return;
    }
    screenShareTrackSource_ = webrtc::make_ref_counted<CallVideoTrackSource>(/*isScreencast=*/true);
    localScreenShareTrack_ = peerConnectionFactory_->CreateVideoTrack(screenShareTrackSource_, kScreenShareTrackId);
    // Тот же паттерн, что и ensureLocalCameraTrack() выше, включая
    // причину подключать только к publishConnection_ (issue #232) — оба
    // трека полностью независимы друг от друга (issue #185).
    if (publishConnection_.connection) {
        attachScreenShareTrack(publishConnection_);
        negotiateLocal(publishConnectionLabel());
    }
}

void CallManager::enableVideo(const QCameraDevice& device) {
    ensureLocalCameraTrack();
    localCameraTrack_->set_enabled(true);
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
    if (localCameraTrack_) {
        localCameraTrack_->set_enabled(false);
    }
}

void CallManager::enableScreenShare(QScreen* screen) {
    ensureLocalScreenShareTrack();
    localScreenShareTrack_->set_enabled(true);
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
    if (localScreenShareTrack_) {
        localScreenShareTrack_->set_enabled(false);
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

    // Срабатывает на собственном потоке воспроизведения ADM (никогда
    // на GUI-потоке Qt) — перепрыгиваем обратно через invokeMethod
    // прежде чем трогать audioOutput_, тот же паттерн, что и у колбэков
    // observer'а PeerConnection ниже.
    audioDeviceModule_ = webrtc::make_ref_counted<CallAudioDeviceModule>(
        [this](const int16_t* samples, size_t frameCount, int /*sampleRateHz*/, size_t channels) {
            // WebRTC отдаёт PCM как int16_t*, QByteArray хочет char* —
            // reinterpret_cast стандартно переходит между этими
            // несвязанными типами указателей (static_cast так не умеет);
            // длина в байтах считается явно рядом, поэтому чтение через
            // переинтерпретированный указатель не выйдет за границы.
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

    // Эхоподавителю WebRTC нужна точная оценка задержки render-сигнала,
    // чтобы синхронизировать по времени то, что сейчас воспроизводится,
    // с тем, что микрофон только что захватил — передавать ему 0 (как
    // раньше делал этот класс) хуже, чем вовсе отключить его, когда
    // реальная буферизация воспроизведения есть (а она есть — потоковый
    // путь AudioOutputDevice). Оставлено на значениях APM по умолчанию
    // (включено), раз теперь задержка сообщается.
    audioDeviceModule_->setTotalDelayMs(AudioOutputDevice::streamingBufferDurationMs());

    const webrtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource =
        peerConnectionFactory_->CreateAudioSource(webrtc::AudioOptions());
    localAudioTrack_ = peerConnectionFactory_->CreateAudioTrack("call-audio0", audioSource.get());
    localAudioTrack_->set_enabled(!muted_);
}

webrtc::scoped_refptr<webrtc::PeerConnectionInterface> CallManager::createPeerConnection(
    const QString& label, std::unique_ptr<PeerObserver>& observerOut) {
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer stunServer;
    stunServer.urls.push_back("stun:stun.l.google.com:19302");
    config.servers.push_back(stunServer);

    observerOut = std::make_unique<PeerObserver>(*this, label);
    webrtc::PeerConnectionDependencies dependencies(observerOut.get());

    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::PeerConnectionInterface>> result =
        peerConnectionFactory_->CreatePeerConnectionOrError(config, std::move(dependencies));
    if (!result.ok()) {
        emit callError(QStringLiteral("Failed to create peer connection for %1: %2")
                           .arg(label, QString::fromUtf8(result.error().message())));
        return nullptr;
    }
    return result.value();
}

void CallManager::closePeerConnection(const QString& peerLogin) {
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end()) {
        return;
    }
    if (it->second.connection) {
        // Синхронно согласно контракту webrtc: после возврата из
        // Close() дальнейших колбэков observer'а не происходит, поэтому
        // после этого безопасно разрушить observer (через peers_.erase()
        // ниже).
        it->second.connection->Close();
    }
    peers_.erase(it);
}

void CallManager::negotiateLocal(const QString& peerLogin) {
    // publishConnection_ (issue #232) переиспользует этот же generic-путь
    // под своей зарезервированной меткой — она не лежит в peers_ (это
    // единственное соединение на весь звонок, а не одно на пира/feed).
    PeerConnectionEntry* entry = (peerLogin == publishConnectionLabel()) ? &publishConnection_ : nullptr;
    if (entry == nullptr) {
        const auto it = peers_.find(peerLogin.toStdString());
        if (it == peers_.end() || !it->second.connection) {
            return;
        }
        entry = &it->second;
    }
    if (!entry->connection) {
        return;
    }
    // Безопасно (пере)согласовывать только из состояний stable (ничего
    // не в ожидании -> создаёт offer) или have-remote-offer (мы только
    // что его получили -> создаёт answer). В остальных случаях
    // пропускаем — в частности, have-local-offer означает, что offer от
    // этой функции уже в полёте (например, AddTrack() вызывает
    // PeerObserver::OnRenegotiationNeeded(), пока явный вызов
    // negotiateLocal() для того же изменения уже выполняется, как
    // происходит для самого первого AddTrack() аудио в
    // ensurePublishConnection()) — повторный вызов SetLocalDescription()
    // поверх него накладывает второй offer и повреждает обмен, что и
    // обнаружило живое тестирование.
    const webrtc::PeerConnectionInterface::SignalingState state = entry->connection->signaling_state();
    if (state != webrtc::PeerConnectionInterface::kStable &&
        state != webrtc::PeerConnectionInterface::kHaveRemoteOffer) {
        return;
    }
    const webrtc::scoped_refptr<LocalDescriptionSetObserver> observer =
        webrtc::make_ref_counted<LocalDescriptionSetObserver>(*this, peerLogin);
    entry->connection->SetLocalDescription(observer);
}

void CallManager::handleLocalDescriptionSet(const QString& peerLogin, bool ok, const QString& errorMessage) {
    if (!ok) {
        emit callError(QStringLiteral("Local description failed for %1: %2").arg(peerLogin, errorMessage));
    }
    // SFU-соединения (issue #232/#233 — единственный путь этого класса
    // теперь) не шлют offer/answer сразу отсюда — у Janus нет отдельного
    // запроса "trickle" (см. doc-комментарий
    // PeerObserver::OnIceGatheringChange()), поэтому ждут полного сбора
    // ICE-кандидатов и уходят из handleIceGatheringComplete() со всеми
    // кандидатами уже внутри SDP.
}

void CallManager::handleIceGatheringComplete(const QString& peerLogin) {
    PeerConnectionEntry* entry = (peerLogin == publishConnectionLabel()) ? &publishConnection_ : nullptr;
    if (entry == nullptr) {
        const auto it = peers_.find(peerLogin.toStdString());
        if (it == peers_.end()) {
            return;
        }
        entry = &it->second;
    }
    // janusHandle < 0 — Janus ещё не назначил handle этой записи
    // (onJanusAttached() не вызывался или ещё не завершился), рано
    // отправлять что-либо.
    if (!entry->connection || entry->janusHandle < 0) {
        return;
    }
    const webrtc::SessionDescriptionInterface* description = entry->connection->local_description();
    if (description == nullptr) {
        return;
    }
    const QJsonObject jsep{
        {"type", description->GetType() == webrtc::SdpType::kOffer ? QStringLiteral("offer") : QStringLiteral("answer")},
        {"sdp", QString::fromStdString(description->ToString())}};

    if (peerLogin == publishConnectionLabel()) {
        // Ни "audio", ни "video" не указаны намеренно — Janus сам решает
        // по m-line'ам в самом SDP, какие медиа принимать; их число
        // здесь заранее неизвестно (может быть только аудио, если видео
        // ещё не включено enableVideo()/enableScreenShare()).
        chatClient_.sendJanusMessage(entry->janusHandle, QJsonObject{{"request", "configure"}}, jsep);
    } else {
        chatClient_.sendJanusMessage(entry->janusHandle, QJsonObject{{"request", "start"}, {"room", sfuRoom_}}, jsep);
    }
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
    // Мы только что применили удалённый offer — отвечаем
    // автоматически созданным answer. Если же мы вместо этого только
    // что применили удалённый answer на наш собственный более ранний
    // offer, signaling-состояние возвращается в stable и делать больше
    // нечего.
    if (it->second.connection->signaling_state() == webrtc::PeerConnectionInterface::kHaveRemoteOffer) {
        negotiateLocal(peerLogin);
    }
}

void CallManager::handleRemoteTrack(const QString& peerLogin,
                                     webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end() || !transceiver || !transceiver->receiver()) {
        return;
    }
    const webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = transceiver->receiver()->track();
    if (!track || track->kind() != webrtc::MediaStreamTrackInterface::kVideoKind) {
        return;
    }
    // Различаем камеру и демонстрацию экрана этого пира по id трека
    // (issue #185) — оба независимы, у каждого свой слот-защита от
    // повторного подключения sink'а.
    const bool isScreenShare = track->id() == kScreenShareTrackId;
    std::unique_ptr<RemoteVideoSink>& sinkSlot =
        isScreenShare ? it->second.remoteScreenShareVideoSink : it->second.remoteCameraVideoSink;
    if (sinkSlot) {
        return;
    }
    sinkSlot = std::make_unique<RemoteVideoSink>(*this, peerLogin, isScreenShare);
    static_cast<webrtc::VideoTrackInterface*>(track.get())->AddOrUpdateSink(sinkSlot.get(), webrtc::VideoSinkWants());
}

void CallManager::handleRemoteVideoFrame(const QString& peerLogin, const QImage& frame, bool isScreenShare) {
    emit remoteVideoFrameReceived(peerLogin, frame, isScreenShare);
}

void CallManager::onCapturedPcm(const QByteArray& data, const QAudioFormat& format) {
    if (!audioDeviceModule_ || !inCall_) {
        return;
    }
    const auto channels = static_cast<size_t>(format.channelCount());
    if (channels == 0) {
        return;
    }
    // Обратная граница той же пары типов, что и в onCapturedPcm-колбэке
    // выше (там int16_t* -> char* для QByteArray, здесь наоборот) —
    // frameCount ниже посчитан из настоящей длины data, так что чтение
    // через переинтерпретированный указатель не выйдет за границы.
    const auto* samples = reinterpret_cast<const int16_t*>(data.constData());
    const size_t frameCount = static_cast<size_t>(data.size()) / sizeof(int16_t) / channels;
    audioDeviceModule_->pushCapturedAudio(samples, frameCount, format.sampleRate(), channels);
}

void CallManager::onCameraFrame(const QVideoFrame& frame) {
    if (!videoEnabled_ || !cameraTrackSource_) {
        return;
    }
    cameraTrackSource_->pushFrame(frame);
}

void CallManager::onScreenShareFrame(const QVideoFrame& frame) {
    if (!screenShareEnabled_ || !screenShareTrackSource_) {
        return;
    }
    screenShareTrackSource_->pushFrame(frame);
}

void CallManager::attachCameraTrack(PeerConnectionEntry& entry) {
    attachTrack(entry, localCameraTrack_, entry.cameraSender);
}

void CallManager::attachScreenShareTrack(PeerConnectionEntry& entry) {
    attachTrack(entry, localScreenShareTrack_, entry.screenShareSender);
}

void CallManager::attachTrack(PeerConnectionEntry& entry, const webrtc::scoped_refptr<webrtc::VideoTrackInterface>& track,
                               webrtc::scoped_refptr<webrtc::RtpSenderInterface>& sender) {
    if (!track || sender || !entry.connection) {
        return;
    }
    const webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> addTrackResult =
        entry.connection->AddTrack(track, std::vector<std::string>{"call-stream"});
    if (!addTrackResult.ok()) {
        emit callError(
            QStringLiteral("Failed to attach video: %1").arg(QString::fromUtf8(addTrackResult.error().message())));
        return;
    }
    sender = addTrackResult.value();
}

void CallManager::onCallPeerJoined(const QString& login) {
    // Чисто информационно (issue #233) — реальное подключение к этому
    // участнику полностью driven событиями Janus (onJanusEvent(),
    // publishers в ответе на собственный join/событие комнаты), а не
    // этим сигналом.
    emit participantJoined(login);
}

void CallManager::onCallPeerLeft(const QString& login) {
    closePeerConnection(login);
    emit participantLeft(login);
    // Оба рода сразу (issue #185) — UI-сторона просто не найдёт плитку
    // того рода, который этот участник в реальности не отправлял, и
    // ничего не сделает для неё.
    emit remoteVideoTrackRemoved(login, /*isScreenShare=*/false);
    emit remoteVideoTrackRemoved(login, /*isScreenShare=*/true);
}

void CallManager::onSfuRoomAssigned(const QString& room) {
    sfuRoom_ = room;
    ensurePublishConnection();
}

void CallManager::ensurePublishConnection() {
    if (publishConnection_.connection || sfuRoom_.isEmpty()) {
        return;
    }
    ensureFactory();

    std::unique_ptr<PeerObserver> observer;
    const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> connection =
        createPeerConnection(publishConnectionLabel(), observer);
    if (!connection) {
        return;
    }
    publishConnection_.connection = connection;
    publishConnection_.observer = std::move(observer);

    if (localAudioTrack_) {
        const webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> addTrackResult =
            publishConnection_.connection->AddTrack(localAudioTrack_, std::vector<std::string>{"call-stream"});
        if (!addTrackResult.ok()) {
            emit callError(QStringLiteral("Failed to attach local audio to SFU publish connection: %1")
                               .arg(QString::fromUtf8(addTrackResult.error().message())));
        }
    }
    // attachCameraTrack()/attachScreenShareTrack() — no-op, если
    // соответствующий трек ещё не создан (enableVideo()/
    // enableScreenShare() ещё не вызывались).
    attachCameraTrack(publishConnection_);
    attachScreenShareTrack(publishConnection_);

    pendingAttach_ = PendingJanusAttach::kPublish;
    chatClient_.sendJanusAttach();
}

void CallManager::ensureSubscribeConnection(const QString& feedId, const QString& peerLogin) {
    if (feedId.isEmpty() || peerLogin.isEmpty() || feedId == ownFeedId_ || peerLogin == localLogin_ ||
        peers_.contains(peerLogin.toStdString())) {
        return;
    }
    if (pendingAttach_ != PendingJanusAttach::kNone) {
        subscribeQueue_.emplace_back(feedId, peerLogin);
        return;
    }
    pendingAttach_ = PendingJanusAttach::kSubscribe;
    pendingSubscribeFeedId_ = feedId;
    pendingSubscribePeerLogin_ = peerLogin;
    chatClient_.sendJanusAttach();
}

void CallManager::processNextQueuedSubscribe() {
    while (pendingAttach_ == PendingJanusAttach::kNone && !subscribeQueue_.empty()) {
        const auto [feedId, peerLogin] = subscribeQueue_.front();
        subscribeQueue_.pop_front();
        if (peers_.contains(peerLogin.toStdString())) {
            continue;  // подписались за это время, пока ждали своей очереди — берём следующего
        }
        pendingAttach_ = PendingJanusAttach::kSubscribe;
        pendingSubscribeFeedId_ = feedId;
        pendingSubscribePeerLogin_ = peerLogin;
        chatClient_.sendJanusAttach();
        return;
    }
}

void CallManager::closeSubscribeConnectionByFeed(const QString& feedId) {
    for (const auto& [login, entry] : peers_) {
        if (entry.sfuFeedId == feedId) {
            closePeerConnection(QString::fromStdString(login));
            return;
        }
    }
}

void CallManager::onJanusAttached(qint64 handle) {
    switch (pendingAttach_) {
        case PendingJanusAttach::kPublish: {
            publishConnection_.janusHandle = handle;
            pendingAttach_ = PendingJanusAttach::kNone;
            chatClient_.sendJanusMessage(
                handle, QJsonObject{{"request", "join"}, {"room", sfuRoom_}, {"ptype", "publisher"}, {"display", localLogin_}});
            processNextQueuedSubscribe();
            break;
        }
        case PendingJanusAttach::kSubscribe: {
            const QString feedId = pendingSubscribeFeedId_;
            const QString peerLogin = pendingSubscribePeerLogin_;
            pendingAttach_ = PendingJanusAttach::kNone;
            pendingSubscribeFeedId_.clear();
            pendingSubscribePeerLogin_.clear();

            std::unique_ptr<PeerObserver> observer;
            const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> connection =
                createPeerConnection(peerLogin, observer);
            if (connection) {
                PeerConnectionEntry entry;
                entry.connection = connection;
                entry.observer = std::move(observer);
                entry.janusHandle = handle;
                entry.sfuFeedId = feedId;
                peers_.emplace(peerLogin.toStdString(), std::move(entry));
                chatClient_.sendJanusMessage(
                    handle, QJsonObject{{"request", "join"}, {"room", sfuRoom_}, {"ptype", "subscriber"}, {"feed", feedId}});
            }
            processNextQueuedSubscribe();
            break;
        }
        case PendingJanusAttach::kNone:
            break;
    }
}

void CallManager::onJanusEvent(const QJsonObject& event) {
    const qint64 sender = event.value(QStringLiteral("sender")).toVariant().toLongLong();
    const QJsonObject data =
        event.value(QStringLiteral("plugindata")).toObject().value(QStringLiteral("data")).toObject();
    const QString videoroom = data.value(QStringLiteral("videoroom")).toString();
    const QJsonValue jsepValue = event.value(QStringLiteral("jsep"));

    if (publishConnection_.janusHandle >= 0 && sender == publishConnection_.janusHandle) {
        if (videoroom == QStringLiteral("joined")) {
            ownFeedId_ = data.value(QStringLiteral("id")).toString();
            for (const QJsonValue& publisherValue : data.value(QStringLiteral("publishers")).toArray()) {
                const QJsonObject publisher = publisherValue.toObject();
                ensureSubscribeConnection(publisher.value(QStringLiteral("id")).toString(),
                                           publisher.value(QStringLiteral("display")).toString());
            }
            negotiateLocal(publishConnectionLabel());
        } else if (videoroom == QStringLiteral("event")) {
            for (const QJsonValue& publisherValue : data.value(QStringLiteral("publishers")).toArray()) {
                const QJsonObject publisher = publisherValue.toObject();
                ensureSubscribeConnection(publisher.value(QStringLiteral("id")).toString(),
                                           publisher.value(QStringLiteral("display")).toString());
            }
            const QString leavingFeed = data.contains(QStringLiteral("leaving"))
                                             ? data.value(QStringLiteral("leaving")).toString()
                                             : data.value(QStringLiteral("unpublished")).toString();
            if (!leavingFeed.isEmpty()) {
                closeSubscribeConnectionByFeed(leavingFeed);
            }
        }
        if (jsepValue.isObject() && publishConnection_.connection) {
            // Ответ Janus на наш "configure" (jsep-answer).
            std::unique_ptr<webrtc::SessionDescriptionInterface> description = webrtc::CreateSessionDescription(
                webrtc::SdpType::kAnswer, jsepValue.toObject().value(QStringLiteral("sdp")).toString().toStdString());
            if (description) {
                const webrtc::scoped_refptr<RemoteDescriptionSetObserver> observer =
                    webrtc::make_ref_counted<RemoteDescriptionSetObserver>(*this, publishConnectionLabel());
                publishConnection_.connection->SetRemoteDescription(std::move(description), observer);
            }
        }
        return;
    }

    // Не publish — событие одной из подписок в peers_ — ищем запись по
    // тому, чей janusHandle совпал с отправителем события.
    if (!jsepValue.isObject()) {
        return;
    }
    for (auto& [login, entry] : peers_) {
        if (entry.janusHandle != sender || !entry.connection) {
            continue;
        }
        // jsep-offer от Janus на нашу подписку ("attached") —
        // handleRemoteDescriptionSet() сам вызовет negotiateLocal() и
        // создаст answer.
        std::unique_ptr<webrtc::SessionDescriptionInterface> description = webrtc::CreateSessionDescription(
            webrtc::SdpType::kOffer, jsepValue.toObject().value(QStringLiteral("sdp")).toString().toStdString());
        if (description) {
            const webrtc::scoped_refptr<RemoteDescriptionSetObserver> observer =
                webrtc::make_ref_counted<RemoteDescriptionSetObserver>(*this, QString::fromStdString(login));
            entry.connection->SetRemoteDescription(std::move(description), observer);
        }
        break;
    }
}

}  // namespace devicehub
