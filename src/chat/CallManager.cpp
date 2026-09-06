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
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState /*newState*/) override {}

    // OnRenegotiationNeeded() намеренно оставлен унаследованным no-op:
    // CallManager всегда точно знает, когда он изменил треки
    // соединения (это именно он вызывает AddTrack()), поэтому вместо
    // этого он согласовывает явно прямо там же — см. enableVideo().
    // Реакция ещё и на это уведомление создавала гонку с этим явным
    // вызовом для самого первого AddTrack() в ensurePeerConnection()
    // (оба попадают в negotiateLocal() для одного и того же пира с
    // разницей в мгновения), накладывая второй offer поверх первого и
    // повреждая обмен — обнаружено живым тестированием, а не просто
    // предполагалось теоретически.

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

    // У null-устройства (например, ничего не выбрано/не перечислено)
    // preferredFormat() полностью нулевой — передача количества
    // каналов 0 в аудио-конвейер WebRTC там приводит к жёсткому крашу
    // (фатальный CHECK), а не к плавному отказу, поэтому это нужно
    // отловить здесь заранее. Звонок при этом всё равно продолжается
    // без локального воспроизведения аудио — mesh-сигналинг от него не
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
    // Самый первый вызов enableVideo()/enableScreenShare(): подключаем
    // (новый) трек ко всем уже существующим соединениям с пирами и
    // явно согласовываем это изменение прямо здесь (тот же паттерн,
    // что уже используют ensurePeerConnection()/onCallRosterReceived()
    // для изначального аудиотрека — почему это остаётся явным, а не
    // реакцией на собственное уведомление WebRTC
    // OnRenegotiationNeeded(), см. doc-комментарий класса). Любое
    // соединение, созданное после этого момента, вместо этого
    // подхватывает трек как часть своего собственного изначального
    // offer/answer (см. ensurePeerConnection()).
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

    // Срабатывает на собственном потоке воспроизведения ADM (никогда
    // на GUI-потоке Qt) — перепрыгиваем обратно через invokeMethod
    // прежде чем трогать audioOutput_, тот же паттерн, что и у колбэков
    // observer'а PeerConnection ниже.
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
    // Часть изначального offer/answer этого соединения, а не отдельная
    // renegotiation — если видеотрек уже существует (даже сейчас
    // выключенный), соединение нового пира включает его с самого
    // начала.
    attachVideoTrack(insertedEntry);
    return &insertedEntry;
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
    const auto it = peers_.find(peerLogin.toStdString());
    if (it == peers_.end() || !it->second.connection) {
        return;
    }
    // Безопасно (пере)согласовывать только из состояний stable (ничего
    // не в ожидании -> создаёт offer) или have-remote-offer (мы только
    // что его получили -> создаёт answer). В остальных случаях
    // пропускаем — в частности, have-local-offer означает, что offer от
    // этой функции уже в полёте (например, AddTrack() вызывает
    // PeerObserver::OnRenegotiationNeeded(), пока явный вызов
    // negotiateLocal() для того же изменения уже выполняется, как
    // происходит для самого первого AddTrack() в
    // ensurePeerConnection()) — повторный вызов SetLocalDescription()
    // поверх него накладывает второй offer и повреждает обмен, что и
    // обнаружило живое тестирование.
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
    // Мы только что применили удалённый offer — отвечаем
    // автоматически созданным answer. Если же мы вместо этого только
    // что применили удалённый answer на наш собственный более ранний
    // offer, signaling-состояние возвращается в stable и делать больше
    // нечего.
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
    // Правило «новый участник всегда отправляет offer»: мы только что
    // присоединились, поэтому инициируем соединение со всеми, кто уже
    // присутствует.
    for (const QString& peerLogin : participants) {
        if (ensurePeerConnection(peerLogin) != nullptr) {
            negotiateLocal(peerLogin);
        }
    }
}

void CallManager::onCallPeerJoined(const QString& login) {
    // Чисто информационно — новый участник инициирует сам, мы просто
    // ждём его offer через onCallSignalReceived().
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
