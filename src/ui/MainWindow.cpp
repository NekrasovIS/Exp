#include "ui/MainWindow.h"

#include "chat/ChannelCrypto.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWidget>

#include <algorithm>
#include <utility>

#include "ui/ChannelsPanel.h"
#include "ui/ChatMessageGrouping.h"
#include "ui/ChatMessageRow.h"
#include "ui/CallWindow.h"
#include "ui/ChatView.h"
#include "ui/CommunitiesPanel.h"
#include "ui/DesktopNotifier.h"
#include "ui/DirectMessageView.h"
#include "ui/FloatingCallTilesOverlay.h"
#include "ui/FooterBar.h"
#include "ui/FriendsPanel.h"
#include "ui/LoginWindow.h"
#include "ui/MemberListPanel.h"
#include "ui/ModeratorsDialog.h"
#include "ui/PinnedMessagesDialog.h"
#include "ui/ProfileDialog.h"
#include "ui/SearchDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr const char* kDefaultAuthServiceUrl = "http://127.0.0.1:8080";
constexpr const char* kDefaultChatServiceWsUrl = "ws://127.0.0.1:8083";
constexpr const char* kDefaultChatServiceUrl = "http://127.0.0.1:8082";
constexpr const char* kDefaultUserServiceUrl = "http://127.0.0.1:8081";
constexpr int kToastTimeoutMs = 4000;
/// За сколько до фактического истечения срока действия access-токена
/// обменивать refresh-токен (issue #105) — небольшой запас, чтобы
/// обмен, выполняемый в фоне, успел завершиться прежде, чем что-либо,
/// использующее lastToken_, начнёт получать 401.
constexpr qint64 kRefreshBufferSeconds = 60;
constexpr int kMessagePageSize = 50;
/// Отражает kMaxAttachmentSizeBytes у chat-service (issue #116) —
/// проверяется и на стороне клиента, чтобы слишком большой файл
/// отклонялся немедленным toast, а не круговым походом на сервер лишь
/// затем, чтобы получить тот же 400.
constexpr qint64 kMaxAttachmentSizeBytes = 5 * 1024 * 1024;
/// Длина превью последнего сообщения под именем канала в сайдбаре
/// (issue #152) — только косметическое ограничение строки, не имеет
/// отношения к лимитам самого тела сообщения.
constexpr int kChannelPreviewMaxChars = 60;

/// Однострочное превью тела сообщения для списка каналов (issue #152):
/// переносы строк схлопываются в пробел, длина ограничена
/// kChannelPreviewMaxChars с многоточием.
QString truncateForChannelPreview(const QString& body) {
    QString flattened = body;
    flattened.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (flattened.size() > kChannelPreviewMaxChars) {
        flattened.truncate(kChannelPreviewMaxChars);
        flattened += QStringLiteral("…");
    }
    return flattened;
}

/// Сводит ChatRestClient::MessageReactionInfo (src/chat) к
/// devicehub::MessageReactionSummary (src/ui/ChatMessageRow.h), поле в
/// поле (issue #333/#334) — та же граница между слоями, что уже
/// проведена для ChatMessageInfo/ChatMessage.
QList<MessageReactionSummary> toReactionSummaries(const QList<MessageReactionInfo>& reactions) {
    QList<MessageReactionSummary> summaries;
    summaries.reserve(reactions.size());
    for (const MessageReactionInfo& reaction : reactions) {
        summaries.push_back(MessageReactionSummary{.emoji = reaction.emoji, .logins = reaction.logins});
    }
    return summaries;
}
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      authClient_(QUrl(qEnvironmentVariable("AUTH_SERVICE_URL", kDefaultAuthServiceUrl))),
      chatClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_WS_URL", kDefaultChatServiceWsUrl))),
      dmChatClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_WS_URL", kDefaultChatServiceWsUrl))),
      chatRestClient_(QUrl(qEnvironmentVariable("CHAT_SERVICE_URL", kDefaultChatServiceUrl))),
      userProfileClient_(QUrl(qEnvironmentVariable("USER_SERVICE_URL", kDefaultUserServiceUrl))) {
    buildUi();
    populateDevices();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setSingleShot(true);
    connect(refreshTimer_, &QTimer::timeout, this, [this]() {
        if (!refreshToken_.isEmpty()) {
            authClient_.refreshAccessToken(refreshToken_);
        }
    });

    connect(settingsDialog_->playToneButton(), &QPushButton::clicked, this, &MainWindow::onPlayToneClicked);
    connect(settingsDialog_->toggleMicButton(), &QPushButton::clicked, this, &MainWindow::onToggleMicClicked);
    connect(settingsDialog_->toggleCameraButton(), &QPushButton::clicked, this, &MainWindow::onToggleCameraClicked);
    connect(settingsDialog_->toggleScreenCaptureButton(), &QPushButton::clicked, this,
            &MainWindow::onToggleScreenCaptureClicked);
    connect(loginWindow_, &LoginWindow::requestCodeRequested, this, &MainWindow::onRequestOtpCodeClicked);
    connect(loginWindow_, &LoginWindow::verifyCodeRequested, this, &MainWindow::onVerifyOtpCodeClicked);
    connect(loginWindow_, &LoginWindow::passwordSignInRequested, this, &MainWindow::onPasswordSignInClicked);
    connect(loginWindow_, &LoginWindow::registerRequested, this, &MainWindow::onRegisterClicked);
    connect(&authClient_, &AuthClient::otpRequested, this,
            [this](const QString& identifier) { loginWindow_->showCodeSent(identifier); });
    // Закрытие окна входа (крестик/Escape) без завершённой авторизации
    // означает, что показывать интерфейс не для кого — приложение
    // завершается, а не остаётся висеть со скрытым пустым MainWindow
    // (issue #156). После успешного входа окно скрывается через hide(),
    // не через reject()/close(), так что в этом случае rejected() не
    // срабатывает.
    connect(loginWindow_, &QDialog::rejected, qApp, &QCoreApplication::quit);
    connect(footerBar_, &FooterBar::accountSettingsRequested, this, &MainWindow::onAccountSettingsClicked);
    connect(profileDialog_, &ProfileDialog::saveRequested, this,
            [this](const ProfileEdits& edits) { userProfileClient_.updateOwnProfile(lastToken_, edits); });
    connect(&userProfileClient_, &UserProfileClient::profileReceived, this, [this](const UserProfile& profile) {
        // fetchProfile() также используется, чтобы искать открытые
        // ключи участников зашифрованного канала (issue #138, см.
        // pendingEncryptedSetup_) — защита от того, чтобы не затереть
        // подвал/диалог редактирования профиля вошедшего пользователя
        // чужим профилем.
        if (profile.login == currentUserLogin_) {
            footerBar_->setProfileText(profile.displayName.isEmpty() ? currentUserLogin_ : profile.displayName);
            profileDialog_->setProfile(profile);
        }
        wrapPendingEncryptedChannelKeyForMember(profile.login, profile.publicKey);
        finishGrantingChannelKeyAccess(profile.login, profile.publicKey);
    });
    connect(&userProfileClient_, &UserProfileClient::profileUpdated, this, [this](const UserProfile& profile) {
        footerBar_->setProfileText(profile.displayName.isEmpty() ? currentUserLogin_ : profile.displayName);
        profileDialog_->setProfile(profile);
        profileDialog_->statusLabel()->setText(tr("Saved"));
    });
    connect(&userProfileClient_, &UserProfileClient::errorOccurred, this, [this](const QString& message) {
        profileDialog_->statusLabel()->setText(tr("Error: %1").arg(message));
        // Общий для профиля и заявок в друзья (issue #187) — в отличие
        // от статус-лейбла ProfileDialog, тост виден независимо от
        // того, открыт ли этот диалог, что важно именно для ошибок
        // заявок в друзья (например, "already friends"), которые
        // случаются в режиме Friends, а не в ProfileDialog.
        showToast(tr("Error: %1").arg(message), ToastBanner::Variant::kError);
    });
    connect(&audioInput_, &AudioInputDevice::levelChanged, settingsDialog_->micLevelBar(), [this](float level) {
        settingsDialog_->micLevelBar()->setValue(static_cast<int>(level * 100.0f));
    });
    connect(&audioInput_, &AudioInputDevice::errorOccurred, this, [this](const QString& message) {
        settingsDialog_->micStatusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&camera_, &CameraDevice::errorOccurred, this, [this](const QString& message) {
        settingsDialog_->cameraStatusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&screenCapture_, &ScreenCaptureDevice::errorOccurred, this, [this](const QString& message) {
        settingsDialog_->screenStatusLabel()->setText(tr("Error: %1").arg(message));
    });
    connect(&authClient_, &AuthClient::tokenReceived, this,
            [this](const QString& token, const QString& refreshToken, qint64 expiresAt) {
                lastToken_ = token;
                refreshToken_ = refreshToken;
                loginWindow_->statusLabel()->setText(tr("Token received, verifying..."));
                authClient_.verifyToken(token);

                // Незаметно обмениваем refresh-токен незадолго до
                // истечения срока действия этого access-токена, вместо
                // того чтобы дожидаться 401 и заставлять пользователя
                // заново входить посреди сессии.
                if (!refreshToken_.isEmpty()) {
                    const qint64 nowSecs = QDateTime::currentSecsSinceEpoch();
                    const qint64 delaySecs = std::max<qint64>(1, expiresAt - nowSecs - kRefreshBufferSeconds);
                    refreshTimer_->start(static_cast<int>(std::min<qint64>(delaySecs, 24 * 3600)) * 1000);
                }
            });
    connect(&authClient_, &AuthClient::tokenVerified, this, [this](bool valid, const QString& subject) {
        currentUserLogin_ = valid ? subject : QString();
        footerBar_->setProfileText(valid ? subject : tr("Not signed in"));
        communitiesPanel_->setCurrentUserLogin(currentUserLogin_);
        channelsPanel_->setCurrentUserLogin(currentUserLogin_);
        chatView_->setCurrentUserLogin(currentUserLogin_);
        memberListPanel_->setCurrentUserLogin(currentUserLogin_);
        // SFU (issue #232): CallManager должен знать собственный логин,
        // чтобы объявить его Janus'у как display при публикации — только
        // так остальные участники смогут сопоставить чужой feed videoroom
        // с логином (Janus сам про логины/каналы ничего не знает).
        callManager_.setLocalLogin(currentUserLogin_);
        if (valid) {
            // Скрываем окно входа и впервые показываем интерфейс — до
            // этого момента MainWindow ни разу не был показан (issue
            // #156, см. main.cpp): единственное видимое окно при
            // запуске — LoginWindow.
            loginWindow_->hide();
            show();
            refreshCommunities();
            // Заполняет отображаемое имя в подвале (до возврата этого
            // запроса используется просто логин выше) и заранее
            // заполняет ProfileDialog на случай клика по Edit Profile.
            userProfileClient_.fetchProfile(lastToken_, currentUserLogin_);

            // E2E-шифрование, фаза 1 (issue #136): убеждаемся, что для
            // этого логина есть локальная пара ключей identity, затем
            // (пере)публикуем её открытую половину. Повторная публикация
            // при каждой проверке намеренно идемпотентна, а не
            // отслеживается/пропускается — PATCH дёшев и безвреден,
            // если значение не изменилось.
            const QString identityKeyDir =
                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/identity-keys");
            identityKeyStore_.emplace(identityKeyDir, currentUserLogin_);
            userProfileClient_.publishPublicKey(lastToken_, identityKeyStore_->publicKeyBase64());
        } else {
            loginWindow_->showError(tr("Token rejected"));
        }
    });
    connect(&authClient_, &AuthClient::errorOccurred, this, [this](const QString& message) {
        // errorOccurred() обслуживает все вызовы AuthClient — пока окно
        // входа ещё видно, ошибка показывается там; после входа (когда
        // это, например, фоновый обмен refresh-токена не удался) —
        // тостом, иначе она осталась бы никем не увиденной в скрытом
        // LoginWindow.
        if (loginWindow_->isVisible()) {
            loginWindow_->showError(message);
        } else {
            showToast(tr("Auth error: %1").arg(message), ToastBanner::Variant::kError);
        }
    });
    connect(&authClient_, &AuthClient::registrationCompleted, this, [this](bool registered) {
        if (!registered) {
            loginWindow_->showError(tr("Registration failed — login already taken"));
        }
        // При успехе сразу после этого срабатывает tokenReceived()
        // (автовход) и доводит метку статуса до "Verified".
    });

    // ChatView сама уже подключает messageEdit()->returnPressed() к
    // sendButton()->click() у себя в конструкторе — повторное
    // подключение здесь заставляло Enter вызывать click() дважды за
    // одно нажатие (issue #211): первый клик отправлял набранный текст
    // и очищал поле, второй — сразу вслед за ним отправлял уже пустое.
    connect(chatView_->sendButton(), &QPushButton::clicked, this, &MainWindow::onSendChatMessageClicked);
    connect(&chatClient_, &ChatClient::subscribed, this,
            [this](qint64 channelId) { chatView_->appendSystemLine(tr("-- subscribed to channel %1 --").arg(channelId)); });
    connect(&chatClient_, &ChatClient::messageReceived, this,
            [this](qint64 id, const QString& author, const QString& body, const QString& sentAt,
                   qint64 attachmentId, const QString& attachmentFilename, qint64 replyToMessageId) {
                const QString displayBody = currentChannelEncrypted_ ? decryptForDisplay(body) : body;
                chatView_->appendMessage(ChatMessage{.id = id,
                                                      .author = author,
                                                      .body = displayBody,
                                                      .sentAt = sentAt,
                                                      .attachmentId = attachmentId,
                                                      .attachmentFilename = attachmentFilename,
                                                      .replyToMessageId = replyToMessageId});
                desktopNotifier_->notifyMessage(author, displayBody, currentUserLogin_);
                // Канал уже открыт — не непрочитанный, но превью в
                // сайдбаре (issue #152) всё равно должно оставаться
                // актуальным без отдельного REST-похода.
                channelsPanel_->recordChannelActivity(
                    selectedChannelId_, id,
                    currentChannelEncrypted_ ? tr("🔒 Encrypted message") : truncateForChannelPreview(displayBody),
                    chat_message_grouping::parseSentAt(sentAt));
            });
    connect(&chatClient_, &ChatClient::messageEdited, this,
            [this](qint64 id, const QString& newBody, const QString& /*editedAt*/) {
                chatView_->updateMessageBody(id, currentChannelEncrypted_ ? decryptForDisplay(newBody) : newBody);
            });
    connect(&chatClient_, &ChatClient::messageDeleted, this,
            [this](qint64 id) { chatView_->removeMessage(id); });
    connect(&chatClient_, &ChatClient::messagePinned, this,
            [this](qint64 id, const QString& pinnedBy, const QString& pinnedAt) {
                chatView_->updatePinned(id, /*isPinned=*/true);
                if (std::none_of(currentPinnedMessages_.cbegin(), currentPinnedMessages_.cend(),
                                  [id](const PinnedMessageInfo& info) { return info.id == id; })) {
                    // Полное содержимое сообщения для панели закреплённых
                    // (issue #338) уже есть в самой строке ChatView — но
                    // раз message_pinned не несёт body/author, надёжнее
                    // просто перезапросить список целиком, чем собирать
                    // его из уже показанной строки, которая может не
                    // существовать (сообщение вне текущей страницы истории).
                    chatRestClient_.listPinnedMessages(lastToken_, selectedChannelId_);
                } else {
                    // Уже в списке (это была идемпотентная повторная
                    // закрепление тем же или другим модератором) — ничего
                    // не меняется, перезапрос не нужен.
                    pinnedMessagesDialog_->setPinnedMessages(currentPinnedMessages_);
                }
                Q_UNUSED(pinnedBy);
                Q_UNUSED(pinnedAt);
            });
    connect(&chatClient_, &ChatClient::messageUnpinned, this, [this](qint64 id) {
        chatView_->updatePinned(id, /*isPinned=*/false);
        currentPinnedMessages_.removeIf([id](const PinnedMessageInfo& info) { return info.id == id; });
        chatView_->setPinnedMessagesCount(currentPinnedMessages_.size());
        pinnedMessagesDialog_->setPinnedMessages(currentPinnedMessages_);
    });
    connect(&chatRestClient_, &ChatRestClient::pinnedMessagesListed, this,
            [this](qint64 channelId, const QList<PinnedMessageInfo>& pinned) {
                if (channelId != selectedChannelId_) {
                    return;
                }
                currentPinnedMessages_ = pinned;
                chatView_->setPinnedMessagesCount(pinned.size());
                pinnedMessagesDialog_->setPinnedMessages(pinned);
                // Issue #339 (найдено при переходе к веб-версии, #340):
                // сообщения, уже закреплённые ДО открытия канала, иначе
                // никогда не получают инлайн-значок "📌 Pinned" в самой
                // ленте — GET .../messages не несёт is_pinned для каждого
                // сообщения (это единственная работа этого отдельного
                // REST-эндпоинта), а строки уже построены к этому моменту
                // с ChatMessage::isPinned по умолчанию false.
                for (const PinnedMessageInfo& info : pinned) {
                    chatView_->updatePinned(info.id, /*isPinned=*/true);
                }
            });
    connect(chatView_, &ChatView::pinnedMessagesToggleRequested, this, [this]() {
        pinnedMessagesDialog_->show();
        pinnedMessagesDialog_->raise();
        pinnedMessagesDialog_->activateWindow();
    });
    connect(pinnedMessagesDialog_, &PinnedMessagesDialog::messageActivated, this,
            [this](qint64 messageId) { chatView_->scrollToMessage(messageId); });
    connect(&chatClient_, &ChatClient::reactionChanged, this,
            [this](qint64 id, const QString& emoji, const QStringList& logins) {
                chatView_->updateReactions(id, emoji, logins);
            });
    connect(chatView_, &ChatView::reactionToggleRequested, this,
            [this](qint64 id, const QString& emoji) { chatClient_.sendToggleReaction(id, emoji); });
    connect(&chatClient_, &ChatClient::errorOccurred, this,
            [this](const QString& message) { chatView_->appendSystemLine(tr("-- error: %1 --").arg(message)); });
    connect(chatView_, &ChatView::typingRequested, this, [this]() { chatClient_.sendTyping(); });
    connect(&chatClient_, &ChatClient::userTyping, this,
            [this](const QString& login) { chatView_->showTypingUser(login); });
    connect(&chatClient_, &ChatClient::onlineMembersReceived, this,
            [this](const QStringList& logins) { memberListPanel_->setOnlineLogins(logins); });
    connect(&chatClient_, &ChatClient::presenceChanged, this,
            [this](const QString& login, bool online) { memberListPanel_->setLoginOnline(login, online); });

    connect(chatView_, &ChatView::callToggleRequested, this, &MainWindow::onCallToggleClicked);
    connect(callWindow_, &CallWindow::muteToggleRequested, this, &MainWindow::onMuteToggleClicked);
    connect(callWindow_, &CallWindow::videoToggleRequested, this, &MainWindow::onVideoToggleClicked);
    connect(callWindow_, &CallWindow::screenShareToggleRequested, this, &MainWindow::onScreenShareToggleClicked);
    // "Leave call" внутри самого окна звонка сводится ровно к тому же
    // действию, что и клик по кнопке звонка в ChatView, когда мы уже в
    // звонке — переиспользуем тот же слот, а не дублируем его тело.
    connect(callWindow_, &CallWindow::leaveCallRequested, this, &MainWindow::onCallToggleClicked);
    connect(callWindow_, &CallWindow::minimizeRequested, this, &MainWindow::onCallMinimizeRequested);
    // Issue #312.
    connect(callWindow_, &CallWindow::reactionRequested, this,
            [this](const QString& emoji) { callManager_.sendReaction(emoji); });
    connect(&callManager_, &CallManager::reactionReceived, this,
            [this](const QString& login, const QString& emoji) { callWindow_->showReaction(login, emoji); });
    connect(floatingCallTilesOverlay_, &FloatingCallTilesOverlay::restoreRequested, this,
            &MainWindow::onCallRestoreRequested);
    connect(chatView_, &ChatView::deleteMessageRequested, this,
            [this](qint64 id) { chatClient_.sendDeleteMessage(id); });
    connect(chatView_, &ChatView::pinMessageRequested, this, [this](qint64 id) { chatClient_.sendPinMessage(id); });
    connect(chatView_, &ChatView::unpinMessageRequested, this,
            [this](qint64 id) { chatClient_.sendUnpinMessage(id); });
    connect(chatView_, &ChatView::attachFileRequested, this, &MainWindow::onAttachFileClicked);
    connect(chatView_, &ChatView::downloadAttachmentRequested, this,
            [this](qint64 attachmentId, const QString& filename) {
                pendingDownloadFilenames_.insert(attachmentId, filename);
                chatRestClient_.downloadAttachment(lastToken_, attachmentId);
            });
    // Issue #188: та же ChatRestClient::downloadAttachment(), что и
    // "Download" выше, только результат идёт в ChatView::
    // setAttachmentPreview() вместо диалога "сохранить на диск" —
    // attachmentDownloaded() ниже различает эти два случая по тому, есть
    // ли @p attachmentId в pendingDownloadFilenames_ (заполняется только
    // настоящим кликом по "Download").
    connect(chatView_, &ChatView::previewAttachmentRequested, this,
            [this](qint64 attachmentId) { chatRestClient_.downloadAttachment(lastToken_, attachmentId); });
    connect(&chatRestClient_, &ChatRestClient::attachmentUploaded, this,
            [this](qint64 id, const QString& /*filename*/) {
                chatClient_.sendMessage(chatView_->messageEdit()->text(), id, chatView_->consumeReplyTarget());
                chatView_->messageEdit()->clear();
            });
    connect(&chatRestClient_, &ChatRestClient::attachmentDownloaded, this,
            [this](qint64 attachmentId, const QByteArray& data) {
                if (!pendingDownloadFilenames_.contains(attachmentId)) {
                    // Фоновая загрузка превью изображения (issue #188),
                    // не клик по "Download" — decode напрямую в строку
                    // сообщения, никакого диалога сохранения. Пустой
                    // QImage() при неудачном decode — setAttachmentPreview()
                    // сама показывает "Preview unavailable" в этом случае.
                    QImage image;
                    image.loadFromData(data);
                    chatView_->setAttachmentPreview(attachmentId, image);
                    return;
                }
                const QString filename = pendingDownloadFilenames_.take(attachmentId);
                const QString savePath =
                    QFileDialog::getSaveFileName(this, tr("Save Attachment"), filename.isEmpty() ? QString() : filename);
                if (savePath.isEmpty()) {
                    return;
                }
                QFile file(savePath);
                if (!file.open(QIODevice::WriteOnly) || file.write(data) < 0) {
                    showToast(tr("Failed to save attachment"), ToastBanner::Variant::kError);
                    return;
                }
            });
    connect(chatView_, &ChatView::openSearchRequested, this, [this]() {
        if (currentChannelEncrypted_) {
            // Подстраховка вдобавок к тому, что ChatView отключает
            // кнопку Search для зашифрованного канала — см.
            // onAttachFileClicked().
            showToast(tr("Search isn't available in encrypted channels"), ToastBanner::Variant::kInfo);
            return;
        }
        searchDialog_->show();
        searchDialog_->raise();
        searchDialog_->activateWindow();
    });
    connect(chatView_, &ChatView::memberListToggleRequested, this,
            [this]() { memberListPanel_->setVisible(memberListPanel_->isHidden()); });
    connect(memberListPanel_, &MemberListPanel::grantChannelKeyAccessRequested, this,
            &MainWindow::grantChannelKeyAccess);
    connect(searchDialog_, &SearchDialog::searchRequested, this, [this](const QString& query) {
        if (selectedChannelId_ < 0 || query.trimmed().isEmpty()) {
            return;
        }
        chatRestClient_.searchMessages(lastToken_, selectedChannelId_, query);
    });
    connect(&chatRestClient_, &ChatRestClient::messagesFound, this,
            [this](qint64 channelId, const QString&, const QList<ChatMessageInfo>& matches) {
                if (channelId == selectedChannelId_) {
                    searchDialog_->setResults(matches);
                }
            });
    connect(searchDialog_, &SearchDialog::resultActivated, this, [this](qint64 messageId) {
        if (!chatView_->scrollToMessage(messageId)) {
            showToast(tr("That message isn't loaded — try \"Load older messages\" first"),
                       ToastBanner::Variant::kInfo);
        }
    });
    connect(&callManager_, &CallManager::participantJoined, this, [this](const QString& login) {
        if (!callParticipants_.contains(login)) {
            callParticipants_.append(login);
        }
        callWindow_->setCallParticipants(callParticipants_);
    });
    connect(&callManager_, &CallManager::participantLeft, this, [this](const QString& login) {
        callParticipants_.removeAll(login);
        callWindow_->setCallParticipants(callParticipants_);
    });
    connect(&callManager_, &CallManager::callError, this,
            [this](const QString& message) { showToast(message, ToastBanner::Variant::kError); });
    connect(&callManager_, &CallManager::remoteVideoFrameReceived, this,
            [this](const QString& login, const QImage& frame, bool isScreenShare) {
                callWindow_->showRemoteVideoFrame(login, frame, isScreenShare);
            });
    connect(&callManager_, &CallManager::remoteVideoTrackRemoved, this,
            [this](const QString& login, bool isScreenShare) { callWindow_->removeRemoteVideo(login, isScreenShare); });

    connect(communitiesPanel_, &CommunitiesPanel::createRequested, this, [this](const QString& name) {
        if (lastToken_.isEmpty()) {
            showToast(tr("Sign in first (Account menu, top right)"), ToastBanner::Variant::kInfo);
            return;
        }
        chatRestClient_.createCommunity(lastToken_, name);
    });
    connect(communitiesPanel_, &CommunitiesPanel::renameRequested, this,
            [this](qint64 id, const QString& newName) { chatRestClient_.renameCommunity(lastToken_, id, newName); });
    connect(communitiesPanel_, &CommunitiesPanel::deleteRequested, this,
            [this](qint64 id) { chatRestClient_.deleteCommunity(lastToken_, id); });
    connect(communitiesPanel_, &CommunitiesPanel::joinByCodeRequested, this, [this](const QString& code) {
        if (lastToken_.isEmpty()) {
            showToast(tr("Sign in first (Account menu, top right)"), ToastBanner::Variant::kInfo);
            return;
        }
        chatRestClient_.joinCommunityByCode(lastToken_, code);
    });
    connect(communitiesPanel_, &CommunitiesPanel::regenerateInviteCodeRequested, this,
            [this](qint64 id) { chatRestClient_.regenerateInviteCode(lastToken_, id); });
    connect(communitiesPanel_, &CommunitiesPanel::communitySelected, this, [this](qint64 id) {
        showCommunitiesMode();
        selectedCommunityId_ = id;
        closeChatView();
        refreshChannelsForSelectedCommunity();
        chatRestClient_.listMembers(lastToken_, id);
        // Issue #338: нужно знать роль вошедшего пользователя в этом
        // сообществе, чтобы решить, показывать ли Pin/Unpin, ещё до
        // того, как он откроет конкретный канал — запрашивается здесь,
        // а не в openChannel()/finishOpeningChannel(), с запасом по
        // времени на сетевой round trip.
        currentCommunityModeratorLogins_.clear();
        chatRestClient_.listModerators(lastToken_, id);
        // Issue #309 — presence for the previous community doesn't
        // apply here; closeChatView() above already dropped chatClient_'s
        // subscription, so no fresh online_members arrives until a
        // channel in this community is opened.
        memberListPanel_->setOnlineLogins({});
    });
    connect(communitiesPanel_, &CommunitiesPanel::friendsRequested, this, &MainWindow::onFriendsButtonClicked);
    connect(communitiesPanel_, &CommunitiesPanel::manageModeratorsRequested, this,
            [this](qint64 id, const QString& name) {
                moderatorsDialog_->setCommunity(id, name);
                moderatorsDialog_->show();
                moderatorsDialog_->raise();
                moderatorsDialog_->activateWindow();
                chatRestClient_.listModerators(lastToken_, id);
            });
    connect(moderatorsDialog_, &ModeratorsDialog::promoteRequested, this,
            [this](qint64 id, const QString& login) { chatRestClient_.promoteModerator(lastToken_, id, login); });
    connect(moderatorsDialog_, &ModeratorsDialog::demoteRequested, this,
            [this](qint64 id, const QString& login) { chatRestClient_.demoteModerator(lastToken_, id, login); });

    connect(friendsPanel_, &FriendsPanel::friendSelected, this, &MainWindow::openDmThreadWith);
    connect(friendsPanel_, &FriendsPanel::addFriendRequested, this, [this](const QString& login) {
        if (lastToken_.isEmpty()) {
            showToast(tr("Sign in first (Account menu, top right)"), ToastBanner::Variant::kInfo);
            return;
        }
        userProfileClient_.sendFriendRequest(lastToken_, login);
    });
    connect(friendsPanel_, &FriendsPanel::acceptRequestRequested, this,
            [this](qint64 requestId) { userProfileClient_.acceptFriendRequest(lastToken_, requestId); });
    connect(friendsPanel_, &FriendsPanel::declineRequestRequested, this,
            [this](qint64 requestId) { userProfileClient_.declineFriendRequest(lastToken_, requestId); });
    connect(friendsPanel_, &FriendsPanel::removeFriendRequested, this,
            [this](const QString& login) { userProfileClient_.removeFriend(lastToken_, login); });

    connect(&userProfileClient_, &UserProfileClient::friendRequestSent, this,
            [this](const QString& recipientLogin, const QString& status) {
                showToast(status == QStringLiteral("accepted")
                              ? tr("You and '%1' are now friends").arg(recipientLogin)
                              : tr("Friend request sent to '%1'").arg(recipientLogin),
                          ToastBanner::Variant::kSuccess);
                userProfileClient_.listFriends(lastToken_);
                userProfileClient_.listIncomingFriendRequests(lastToken_);
            });
    connect(&userProfileClient_, &UserProfileClient::incomingFriendRequestsListed, this,
            [this](const QList<FriendRequestInfo>& requests) { friendsPanel_->setIncomingRequests(requests); });
    connect(&userProfileClient_, &UserProfileClient::friendRequestAccepted, this, [this](qint64) {
        showToast(tr("Friend request accepted"), ToastBanner::Variant::kSuccess);
        userProfileClient_.listFriends(lastToken_);
        userProfileClient_.listIncomingFriendRequests(lastToken_);
    });
    connect(&userProfileClient_, &UserProfileClient::friendRequestDeclined, this, [this](qint64) {
        userProfileClient_.listIncomingFriendRequests(lastToken_);
    });
    connect(&userProfileClient_, &UserProfileClient::friendsListed, this,
            [this](const QStringList& logins) { friendsPanel_->setFriends(logins); });
    connect(&userProfileClient_, &UserProfileClient::friendRemoved, this, [this](const QString&) {
        userProfileClient_.listFriends(lastToken_);
    });

    connect(&chatRestClient_, &ChatRestClient::dmThreadOpened, this, [this](qint64 id, const QString& otherLogin) {
        openDmThreadId_ = id;
        openDmOtherLogin_ = otherLogin;
        directMessageView_->showThread(otherLogin);
        // REST — разовая загрузка истории; дальнейшая доставка новых
        // сообщений идёт через dmChatClient_ (issue #187, Фаза 2b) —
        // те же роли, что у chatRestClient_.listMessages()/chatClient_
        // для канала.
        chatRestClient_.listDirectMessages(lastToken_, id, /*limit=*/50);
        dmChatClient_.disconnectFromChannel();
        dmChatClient_.connectToDirectMessageThread(lastToken_, id);
    });
    connect(&chatRestClient_, &ChatRestClient::directMessagesListed, this,
            [this](qint64 threadId, const QList<DirectMessageInfo>& messages) {
                if (threadId != openDmThreadId_) {
                    return;
                }
                directMessageView_->setMessages(messages);
            });
    connect(&dmChatClient_, &ChatClient::messageReceived, this,
            [this](qint64 id, const QString& author, const QString& body, const QString& sentAt, qint64 /*attachmentId*/,
                   const QString& /*attachmentFilename*/) {
                directMessageView_->appendMessage(
                    DirectMessageInfo{.id = id, .author = author, .body = body, .sentAt = sentAt});
            });
    connect(&dmChatClient_, &ChatClient::errorOccurred, this,
            [this](const QString& message) { showToast(message, ToastBanner::Variant::kError); });
    connect(directMessageView_, &DirectMessageView::sendMessageRequested, this, [this](const QString& body) {
        if (openDmThreadId_ >= 0) {
            dmChatClient_.sendMessage(body);
        }
    });
    // Issue #313 — same shape as chatClient_'s typing wiring above.
    connect(directMessageView_, &DirectMessageView::typingRequested, this, [this]() { dmChatClient_.sendTyping(); });
    connect(&dmChatClient_, &ChatClient::userTyping, this,
            [this](const QString& login) { directMessageView_->showTypingUser(login); });

    connect(channelsPanel_, &ChannelsPanel::createRequested, this, [this](const QString& name, bool isEncrypted) {
        if (selectedCommunityId_ < 0) {
            showToast(tr("Pick a community first"), ToastBanner::Variant::kInfo);
            return;
        }
        chatRestClient_.createChannel(lastToken_, selectedCommunityId_, name, isEncrypted);
    });
    connect(channelsPanel_, &ChannelsPanel::renameRequested, this,
            [this](qint64 id, const QString& newName) { chatRestClient_.renameChannel(lastToken_, id, newName); });
    connect(channelsPanel_, &ChannelsPanel::deleteRequested, this,
            [this](qint64 id) { chatRestClient_.deleteChannel(lastToken_, id); });
    connect(channelsPanel_, &ChannelsPanel::channelSelected, this,
            [this](qint64 id, const QString& name) { openChannel(id, name); });

    connect(chatView_, &ChatView::createChannelRequested, channelsPanel_->addButton(), &QPushButton::click);

    connect(&chatRestClient_, &ChatRestClient::communityCreated, this,
            [this](qint64 id, const QString& name, const QString& inviteCode) {
                // Код приглашения показан сразу здесь (issue #186) —
                // тем не менее доступен и позже через "Copy Invite Code"
                // в контекстном меню сообщества, тост не единственный
                // способ его увидеть.
                showToast(tr("Community '%1' created — invite code: %2").arg(name, inviteCode),
                           ToastBanner::Variant::kSuccess);
                pendingCommunitySelection_ = id;
                refreshCommunities();
            });
    connect(&chatRestClient_, &ChatRestClient::joinedCommunityByCode, this,
            [this](qint64 id, const QString& name) {
                showToast(tr("Joined community '%1'").arg(name), ToastBanner::Variant::kSuccess);
                pendingCommunitySelection_ = id;
                refreshCommunities();
            });
    connect(&chatRestClient_, &ChatRestClient::inviteCodeRegenerated, this,
            [this](qint64, const QString& inviteCode) {
                showToast(tr("New invite code: %1").arg(inviteCode), ToastBanner::Variant::kSuccess);
                refreshCommunities();
            });
    connect(&chatRestClient_, &ChatRestClient::communitiesListed, this, [this](const QList<ChatItem>& communities) {
        communities_ = communities;
        communitiesPanel_->setCommunities(communities_);
        if (pendingCommunitySelection_ >= 0) {
            communitiesPanel_->selectCommunityId(pendingCommunitySelection_);
            selectedCommunityId_ = pendingCommunitySelection_;
            refreshChannelsForSelectedCommunity();
        }
        pendingCommunitySelection_ = -1;
    });
    connect(&chatRestClient_, &ChatRestClient::communityRenamed, this, [this](qint64, const QString& newName) {
        showToast(tr("Community renamed to '%1'").arg(newName), ToastBanner::Variant::kSuccess);
        refreshCommunities();
    });
    connect(&chatRestClient_, &ChatRestClient::communityDeleted, this, [this](qint64 id) {
        showToast(tr("Community deleted"), ToastBanner::Variant::kSuccess);
        if (id == selectedCommunityId_) {
            selectedCommunityId_ = -1;
            channelsPanel_->setChannels({});
            closeChatView();
        }
        refreshCommunities();
    });
    connect(&chatRestClient_, &ChatRestClient::moderatorPromoted, this, [this](qint64 id, const QString& login) {
        moderatorsDialog_->statusLabel()->setText(tr("Promoted '%1'").arg(login));
        chatRestClient_.listModerators(lastToken_, id);
    });
    connect(&chatRestClient_, &ChatRestClient::moderatorDemoted, this, [this](qint64 id, const QString& login) {
        moderatorsDialog_->statusLabel()->setText(tr("Demoted '%1'").arg(login));
        chatRestClient_.listModerators(lastToken_, id);
    });
    connect(&chatRestClient_, &ChatRestClient::moderatorsListed, this,
            [this](qint64 id, const QStringList& logins) {
                if (id == moderatorsDialog_->communityId()) {
                    moderatorsDialog_->setModerators(logins);
                }
                // Issue #338: та же рассылка, но для роли вошедшего
                // пользователя в открытом(-ываемом) канале — независимо
                // от того, открыт ли ModeratorsDialog сейчас.
                if (id == selectedCommunityId_) {
                    currentCommunityModeratorLogins_ = logins;
                    chatView_->setCanManageChannel(currentUserCanManageChannel());
                }
            });
    connect(&chatRestClient_, &ChatRestClient::communityJoined, this, [this](qint64 id) {
        showToast(tr("Joined community"), ToastBanner::Variant::kSuccess);
        // Список участников не обновляется сам — без этого только что
        // присоединившийся пользователь не появляется в MemberListPanel,
        // пока кто-нибудь не переоткроет сообщество (см. также
        // membersListed() ниже, тот же фильтр по selectedCommunityId_).
        if (id == selectedCommunityId_) {
            chatRestClient_.listMembers(lastToken_, id);
        }
    });
    connect(&chatRestClient_, &ChatRestClient::channelCreated, this,
            [this](qint64 id, const QString& name, bool isEncrypted) {
                showToast(tr("Channel '%1' created").arg(name), ToastBanner::Variant::kSuccess);
                if (isEncrypted && identityKeyStore_.has_value()) {
                    // Генерируем симметричный ключ канала здесь —
                    // chat-service никогда его не видит, только
                    // обёрнутые копии для каждого участника (issue #138).
                    const QByteArray channelKey = channel_crypto::generateChannelKey();
                    channelKeys_[id] = channelKey;
                    pendingEncryptedSetup_ = PendingEncryptedChannelSetup{.channelId = id, .channelKey = channelKey};
                    // Оборачиваем для себя сразу — не нужен круговой
                    // запрос, наш собственный открытый ключ уже доступен
                    // локально.
                    const QString ownWrapped = channel_crypto::wrapKeyForRecipient(
                        channelKey, QByteArray::fromBase64(identityKeyStore_->publicKeyBase64().toUtf8()));
                    chatRestClient_.setChannelKey(lastToken_, id, currentUserLogin_, ownWrapped);
                    // Каждому другому текущему участнику тоже нужна своя
                    // обёрнутая копия — разрешается по мере поступления
                    // ответов membersListed()/profileReceived().
                    chatRestClient_.listMembers(lastToken_, selectedCommunityId_);
                }
                pendingChannelSelection_ = id;
                refreshChannelsForSelectedCommunity();
            });
    connect(&chatRestClient_, &ChatRestClient::membersListed, this,
            [this](qint64 communityId, const QStringList& logins) {
                if (communityId == selectedCommunityId_) {
                    memberListPanel_->setMembers(logins);
                }
                if (!pendingEncryptedSetup_.has_value()) {
                    return;  // Не связано с текущим созданием зашифрованного канала.
                }
                for (const QString& login : logins) {
                    if (login != currentUserLogin_) {
                        pendingEncryptedSetup_->pendingMemberLogins.insert(login);
                        userProfileClient_.fetchProfile(lastToken_, login);
                    }
                }
                if (pendingEncryptedSetup_->pendingMemberLogins.isEmpty()) {
                    pendingEncryptedSetup_.reset();  // Сообщество без других участников — оборачивать только для себя.
                }
            });
    connect(&chatRestClient_, &ChatRestClient::channelKeySet, this, [](qint64, const QString&) {
        // Подтверждение в стиле fire-and-forget —
        // wrapPendingEncryptedChannelKeyForMember() уже обновляет учёт в
        // pendingEncryptedSetup_ в момент вызова setChannelKey(), а не
        // когда приходит этот ответ (потерянный здесь ответ на этом
        // этапе не повторяется — см. известные ограничения issue #138).
    });
    connect(&chatRestClient_, &ChatRestClient::channelsListed, this, [this](const QList<ChatItem>& channels) {
        channels_ = channels;
        channelsPanel_->setChannels(channels_);
        // Превью последнего сообщения + сортировка по активности в
        // сайдбаре (issue #152) — единственный способ узнать о канале,
        // который сейчас не открыт (ChatClient подписан ровно на один
        // канал одновременно, см. его doc-комментарий), не трогая
        // chat-service: свой отдельный REST-запрос на канал (см.
        // ChatRestClient::fetchLatestMessage()), ответ приходит через
        // latestMessageFetched() выше, а не через messagesListed() —
        // чтобы никогда не перепутаться с реальной подгрузкой истории,
        // если пользователь откроет один из этих каналов раньше, чем
        // придёт ответ на его превью.
        for (const ChatItem& channel : channels_) {
            chatRestClient_.fetchLatestMessage(lastToken_, channel.id);
        }
        if (pendingChannelSelection_ >= 0) {
            channelsPanel_->selectChannelId(pendingChannelSelection_);
            const auto it = std::find_if(channels_.cbegin(), channels_.cend(),
                                          [this](const ChatItem& item) { return item.id == pendingChannelSelection_; });
            if (it != channels_.cend()) {
                openChannel(it->id, it->name);
            }
        }
        pendingChannelSelection_ = -1;
    });
    connect(&chatRestClient_, &ChatRestClient::channelRenamed, this, [this](qint64 id, const QString& newName) {
        showToast(tr("Channel renamed to '%1'").arg(newName), ToastBanner::Variant::kSuccess);
        if (id == selectedChannelId_) {
            chatView_->showChannel(newName);
        }
        refreshChannelsForSelectedCommunity();
    });
    connect(&chatRestClient_, &ChatRestClient::channelDeleted, this, [this](qint64 id) {
        showToast(tr("Channel deleted"), ToastBanner::Variant::kSuccess);
        if (id == selectedChannelId_) {
            closeChatView();
        }
        refreshChannelsForSelectedCommunity();
    });
    connect(&chatRestClient_, &ChatRestClient::myChannelKeyFetched, this,
            [this](qint64 channelId, const QString& wrappedKey) {
                if (identityKeyStore_.has_value()) {
                    const std::optional<QByteArray> unwrapped = channel_crypto::unwrapKey(
                        wrappedKey, QByteArray::fromBase64(identityKeyStore_->publicKeyBase64().toUtf8()),
                        identityKeyStore_->secretKeyBytes());
                    if (unwrapped.has_value()) {
                        channelKeys_[channelId] = *unwrapped;
                    } else {
                        showToast(tr("Failed to decrypt this channel's key"), ToastBanner::Variant::kError);
                    }
                }
                if (channelId == selectedChannelId_) {
                    finishOpeningChannel(channelId);
                }
            });
    connect(&chatRestClient_, &ChatRestClient::myChannelKeyNotFound, this, [this](qint64 channelId) {
        if (channelId == selectedChannelId_) {
            showToast(tr("You don't have access to this encrypted channel yet — ask a member with access to grant it"),
                       ToastBanner::Variant::kInfo);
            finishOpeningChannel(channelId);
        }
    });
    connect(&chatRestClient_, &ChatRestClient::latestMessageFetched, this,
            [this](qint64 channelId, const QList<ChatMessageInfo>& messages) {
                // Фоновый запрос превью для сайдбара (issue #152) — своя
                // ветка, отдельная от messagesListed()/подгрузки истории
                // открытого канала ниже, специально чтобы ответ на этот
                // запрос никогда не мог быть перепутан с ответом на
                // реальную подгрузку истории для того же канала, даже
                // если оба запроса оказались в полёте одновременно (см.
                // doc-комментарий ChatRestClient::fetchLatestMessage()).
                if (messages.isEmpty()) {
                    return;
                }
                const ChatMessageInfo& latest = messages.first();
                const auto it = std::find_if(channels_.cbegin(), channels_.cend(),
                                              [channelId](const ChatItem& item) { return item.id == channelId; });
                const bool isEncrypted = it != channels_.cend() && it->isEncrypted;
                channelsPanel_->recordChannelActivity(
                    channelId, latest.id, isEncrypted ? tr("🔒 Encrypted message") : truncateForChannelPreview(latest.body),
                    chat_message_grouping::parseSentAt(latest.sentAt));
            });
    connect(&chatRestClient_, &ChatRestClient::messagesListed, this,
            [this](qint64 channelId, const QList<ChatMessageInfo>& messages) {
                if (channelId != selectedChannelId_) {
                    return;  // Устаревший ответ для канала, который мы уже покинули.
                }
                QList<ChatMessage> converted;
                converted.reserve(messages.size());
                for (const ChatMessageInfo& info : messages) {
                    converted.append(ChatMessage{.id = info.id,
                                                  .author = info.author,
                                                  .body = currentChannelEncrypted_ ? decryptForDisplay(info.body) : info.body,
                                                  .sentAt = info.sentAt,
                                                  .attachmentId = info.attachmentId,
                                                  .attachmentFilename = info.attachmentFilename,
                                                  .reactions = toReactionSummaries(info.reactions),
                                                  .replyToMessageId = info.replyToMessageId});
                }
                if (oldestMessageId_ < 0) {
                    // Первоначальная загрузка истории для этого канала —
                    // список был пуст, поэтому добавление в конец в
                    // хронологическом порядке (как пришло) выглядит так
                    // же, как и вставка в начало.
                    for (const ChatMessage& message : converted) {
                        chatView_->appendMessage(message);
                    }
                } else {
                    chatView_->prependMessages(converted);
                }
                if (!converted.isEmpty()) {
                    oldestMessageId_ = converted.first().id;
                }
                chatView_->setLoadOlderVisible(messages.size() == kMessagePageSize);
            });
    connect(chatView_, &ChatView::loadOlderMessagesRequested, this, [this]() {
        if (selectedChannelId_ >= 0) {
            chatRestClient_.listMessages(lastToken_, selectedChannelId_, kMessagePageSize, oldestMessageId_);
        }
    });
    connect(&chatRestClient_, &ChatRestClient::errorOccurred, this, [this](const QString& message) {
        showToast(tr("Error: %1").arg(message), ToastBanner::Variant::kError);
    });

    connect(footerBar_->settingsButton(), &QPushButton::clicked, this, [this]() {
        settingsDialog_->show();
        settingsDialog_->raise();
        settingsDialog_->activateWindow();
    });

    // CameraDevice владеет единственным video sink, который допускает
    // QMediaCaptureSession (см. её doc-комментарий) — виджет превью
    // получает кадры вручную, вместо того чтобы быть подключённым как
    // выход сессии напрямую, так что те же кадры также доступны для
    // исходящего видео звонка (issue #72).
    connect(&camera_, &CameraDevice::frameAvailable, this,
            [this](const QVideoFrame& frame) { settingsDialog_->videoPreview()->videoSink()->setVideoFrame(frame); });
    // Тот же разветвлённый вывод, третий потребитель: собственная
    // плитка локального превью звонка (issue #91). Кадры реально идут
    // только пока камера работает (превью в Settings или
    // CallManager::enableVideo()), поэтому это безопасно оставить
    // подключённым безусловно.
    connect(&camera_, &CameraDevice::frameAvailable, this,
            [this](const QVideoFrame& frame) { callWindow_->localVideoWidget()->videoSink()->setVideoFrame(frame); });
    // Тот же рефакторинг "владеет единственным sink, ретранслирует", что
    // и у CameraDevice (issue #112). Демонстрация экрана и видео с
    // камеры — независимые треки с issue #185, у каждого своя плитка
    // локального превью (localScreenShareVideoWidget() против
    // localVideoWidget()), а не общая.
    connect(&screenCapture_, &ScreenCaptureDevice::frameAvailable, this,
            [this](const QVideoFrame& frame) { settingsDialog_->screenPreview()->videoSink()->setVideoFrame(frame); });
    connect(&screenCapture_, &ScreenCaptureDevice::frameAvailable, this, [this](const QVideoFrame& frame) {
        callWindow_->localScreenShareVideoWidget()->videoSink()->setVideoFrame(frame);
    });

    // Issue #156: токен между запусками не сохраняется (каждый
    // запуск стартует разлогиненным), так что условие "ещё не
    // авторизован" сводится к "всегда показывать это при старте".
    // MainWindow при этом ни разу не показывается здесь — main.cpp не
    // вызывает show() для него, единственное видимое окно на старте
    // это LoginWindow; сам MainWindow показывается только из
    // обработчика tokenVerified(true, ...) выше, после успешного входа
    // по коду или по паролю/регистрации.
    loginWindow_->show();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    setWindowTitle(tr("DeviceHub"));
    resize(1280, 800);
    setMinimumSize(900, 600);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* sidebar = new QWidget(central);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    // 72px иконочная полоса сообществ + 240px список каналов
    // (issue #182 — расположение/размеры, не цвета).
    sidebar->setFixedWidth(312);
    auto* sidebarLayout = new QHBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);
    communitiesPanel_ = new CommunitiesPanel(sidebar);
    // Собственный host, а не sidebar целиком (issue #216) — FriendsPanel
    // всплывает поверх *своего* родителя целиком (см. её doc-комментарий),
    // а перекрывать иконочную полосу сообществ она не должна: та обязана
    // оставаться видимой и кликабельной, пока открыта панель друзей.
    auto* channelsHost = new QWidget(sidebar);
    auto* channelsHostLayout = new QVBoxLayout(channelsHost);
    channelsHostLayout->setContentsMargins(0, 0, 0, 0);
    channelsPanel_ = new ChannelsPanel(channelsHost);
    channelsHostLayout->addWidget(channelsPanel_);
    friendsPanel_ = new FriendsPanel(channelsHost);
    sidebarLayout->addWidget(communitiesPanel_);
    sidebarLayout->addWidget(channelsHost, /*stretch=*/1);

    chatView_ = new ChatView(central);
    directMessageView_ = new DirectMessageView(central);
    contentStack_ = new QStackedWidget(central);
    contentStack_->addWidget(chatView_);
    contentStack_->addWidget(directMessageView_);
    // Родитель — contentStack_, а не chatView_ (issue #187, Фаза 3):
    // тост должен быть виден и в режиме Friends, когда показан
    // directMessageView_, а не chatView_ — ToastBanner сам следит за
    // resize() своего parentWidget() (см. её конструктор), так что
    // достаточно просто выбрать родителя, который виден в обоих режимах.
    toastBanner_ = new ToastBanner(contentStack_);
    desktopNotifier_ = new DesktopNotifier(this, this);

    // Отдельное окно звонка (issue #185) — MainWindow владеет её временем
    // жизни через дерево QObject (this как родитель), показывает/скрывает
    // по фактическому входу/выходу из звонка.
    callWindow_ = new CallWindow(this);
    floatingCallTilesOverlay_ = new FloatingCallTilesOverlay(this);

    // Список участников сообщества справа от чата — элемент раскладки,
    // которого раньше не было вовсе (issue #182); та же 240px ширина,
    // что и список каналов.
    memberListPanel_ = new MemberListPanel(central);
    memberListPanel_->setFixedWidth(240);

    auto* middleLayout = new QHBoxLayout;
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->addWidget(sidebar);
    middleLayout->addWidget(contentStack_, /*stretch=*/1);
    middleLayout->addWidget(memberListPanel_);

    footerBar_ = new FooterBar(central);

    rootLayout->addLayout(middleLayout, /*stretch=*/1);
    rootLayout->addWidget(footerBar_);

    setCentralWidget(central);

    settingsDialog_ = new SettingsDialog(this);
    profileDialog_ = new ProfileDialog(this);
    moderatorsDialog_ = new ModeratorsDialog(this);
    searchDialog_ = new SearchDialog(this);
    pinnedMessagesDialog_ = new PinnedMessagesDialog(this);
    loginWindow_ = new LoginWindow(this);
}

void MainWindow::populateDevices() {
    for (const QAudioDevice& device : enumerator_.audioOutputs()) {
        settingsDialog_->outputCombo()->addItem(device.description(), QVariant::fromValue(device));
    }
    for (const QAudioDevice& device : enumerator_.audioInputs()) {
        settingsDialog_->inputCombo()->addItem(device.description(), QVariant::fromValue(device));
    }
    for (const QCameraDevice& device : enumerator_.cameras()) {
        settingsDialog_->cameraCombo()->addItem(device.description(), QVariant::fromValue(device));
    }

    screens_ = enumerator_.screens();
    for (const QScreen* screen : std::as_const(screens_)) {
        settingsDialog_->screenCombo()->addItem(screen->name());
    }
}

void MainWindow::onPlayToneClicked() {
    const QAudioDevice device = settingsDialog_->outputCombo()->currentData().value<QAudioDevice>();
    audioOutput_.playTestTone(device);
}

void MainWindow::onToggleMicClicked() {
    if (audioInput_.isCapturing()) {
        audioInput_.stop();
        settingsDialog_->toggleMicButton()->setText(tr("Start capture"));
        settingsDialog_->micLevelBar()->setValue(0);
    } else {
        settingsDialog_->micStatusLabel()->clear();
        const QAudioDevice device = settingsDialog_->inputCombo()->currentData().value<QAudioDevice>();
        audioInput_.start(device);
        settingsDialog_->toggleMicButton()->setText(tr("Stop capture"));
    }
}

void MainWindow::onToggleCameraClicked() {
    if (camera_.isActive()) {
        camera_.stop();
        settingsDialog_->toggleCameraButton()->setText(tr("Start camera"));
        return;
    }

    if (settingsDialog_->cameraCombo()->currentIndex() < 0) {
        settingsDialog_->cameraStatusLabel()->setText(tr("No camera available"));
        return;
    }

    settingsDialog_->cameraStatusLabel()->clear();
    const QCameraDevice device = settingsDialog_->cameraCombo()->currentData().value<QCameraDevice>();
    camera_.setDevice(device);
    camera_.start();
    settingsDialog_->toggleCameraButton()->setText(tr("Stop camera"));
}

void MainWindow::onToggleScreenCaptureClicked() {
    if (screenCapture_.isActive()) {
        screenCapture_.stop();
        settingsDialog_->toggleScreenCaptureButton()->setText(tr("Start screen capture"));
        return;
    }

    if (const int index = settingsDialog_->screenCombo()->currentIndex(); index >= 0 && index < screens_.size()) {
        settingsDialog_->screenStatusLabel()->clear();
        screenCapture_.setScreen(screens_[index]);
        screenCapture_.start();
        settingsDialog_->toggleScreenCaptureButton()->setText(tr("Stop screen capture"));
    } else {
        settingsDialog_->screenStatusLabel()->setText(tr("No screen available"));
    }
}

void MainWindow::onPasswordSignInClicked(const QString& login, const QString& password) {
    loginWindow_->statusLabel()->setText(tr("Requesting token..."));
    authClient_.requestToken(login, password);
}

void MainWindow::onRegisterClicked(const QString& login, const QString& password) {
    loginWindow_->statusLabel()->setText(tr("Registering..."));
    authClient_.registerUser(login, password);
}

void MainWindow::onSendChatMessageClicked() {
    if (selectedChannelId_ < 0) {
        return;
    }
    const QString text = chatView_->messageEdit()->text();
    if (text.isEmpty()) {
        return;
    }
    QString outgoing = text;
    if (currentChannelEncrypted_) {
        const auto it = channelKeys_.constFind(selectedChannelId_);
        if (it == channelKeys_.constEnd()) {
            showToast(tr("No key for this channel yet — can't send"), ToastBanner::Variant::kError);
            return;
        }
        outgoing = channel_crypto::encryptMessage(text, it.value());
    }
    if (chatView_->editingMessageId() >= 0) {
        chatClient_.sendEditMessage(chatView_->editingMessageId(), outgoing);
        chatView_->cancelEditingMessage();
    } else {
        chatClient_.sendMessage(outgoing, /*attachmentId=*/-1, chatView_->consumeReplyTarget());
        chatView_->messageEdit()->clear();
    }
}

void MainWindow::onAttachFileClicked() {
    if (selectedChannelId_ < 0) {
        return;
    }
    if (currentChannelEncrypted_) {
        // Подстраховка вдобавок к тому, что ChatView отключает кнопку
        // Attach для зашифрованного канала — вложения пока не
        // шифруются на стороне клиента (issue #138), chat-service тоже
        // отклоняет загрузку с 400.
        showToast(tr("Attachments aren't supported in encrypted channels yet"), ToastBanner::Variant::kInfo);
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Attach File"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (file.size() > kMaxAttachmentSizeBytes) {
        showToast(tr("Attachment exceeds the 5 MB size limit"), ToastBanner::Variant::kError);
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        showToast(tr("Failed to read file"), ToastBanner::Variant::kError);
        return;
    }
    const QByteArray data = file.readAll();
    const QString contentType = QMimeDatabase().mimeTypeForFile(path).name();
    // Загружает немедленно, затем attachmentUploaded() автоматически
    // отправляет его как сообщение (issue #116) — намеренное упрощение
    // по сравнению с двухшаговым сценарием "прикрепить, просмотреть,
    // затем нажать Send".
    chatRestClient_.uploadAttachment(lastToken_, selectedChannelId_, QFileInfo(path).fileName(), contentType, data);
}

void MainWindow::onCallToggleClicked() {
    if (callManager_.inCall()) {
        leaveCallIfActive();
        return;
    }
    const QAudioDevice inputDevice = settingsDialog_->inputCombo()->currentData().value<QAudioDevice>();
    const QAudioDevice outputDevice = settingsDialog_->outputCombo()->currentData().value<QAudioDevice>();
    callManager_.joinCall(inputDevice, outputDevice);
    chatView_->setCallState(true);
    callWindow_->setMuted(callManager_.isMuted());
    callWindow_->setVideoEnabled(false);
    callWindow_->setScreenShareEnabled(false);
    callWindow_->show();
    callWindow_->raise();
    callWindow_->activateWindow();
}

void MainWindow::onCallMinimizeRequested() {
    // issue #287: без явного move() мини-окно открывается там, где
    // решит оконный менеджер по умолчанию — никак не привязано к тому,
    // где только что было CallWindow, что ощущается как "появилось не
    // там". Читаем geometry() до hide() — после hide() она у скрытого
    // окна на некоторых платформах не гарантированно валидна.
    floatingCallTilesOverlay_->move(callWindow_->geometry().topLeft());
    callWindow_->detachTilesTo(floatingCallTilesOverlay_->canvas());
    callWindow_->hide();
    floatingCallTilesOverlay_->show();
    floatingCallTilesOverlay_->raise();
    // activateWindow() рядом с raise() (issue #287) — raise() один
    // только поднимает окно в z-order, не гарантируя ему фокус или то,
    // что оконный менеджер реально выведет его поверх остальных (тот
    // же паттерн уже применяется в onCallRestoreRequested() ниже).
    floatingCallTilesOverlay_->activateWindow();
}

void MainWindow::onCallRestoreRequested() {
    callWindow_->reattachTiles();
    floatingCallTilesOverlay_->hide();
    callWindow_->show();
    callWindow_->raise();
    callWindow_->activateWindow();
}

void MainWindow::onMuteToggleClicked() {
    callManager_.setMuted(!callManager_.isMuted());
    callWindow_->setMuted(callManager_.isMuted());
}

void MainWindow::onVideoToggleClicked() {
    if (callManager_.videoEnabled()) {
        callManager_.disableVideo();
    } else {
        const QCameraDevice cameraDevice = settingsDialog_->cameraCombo()->currentData().value<QCameraDevice>();
        callManager_.enableVideo(cameraDevice);
    }
    // Независимо от screenShareEnabled_ (issue #185 — оба трека
    // независимы), поэтому обновляем только состояние видео.
    callWindow_->setVideoEnabled(callManager_.videoEnabled());
}

void MainWindow::onScreenShareToggleClicked() {
    if (callManager_.screenShareEnabled()) {
        callManager_.disableScreenShare();
    } else if (const int index = settingsDialog_->screenCombo()->currentIndex();
               index >= 0 && index < screens_.size()) {
        callManager_.enableScreenShare(screens_[index]);
    } else {
        showToast(tr("No screen available"), ToastBanner::Variant::kError);
    }
    callWindow_->setScreenShareEnabled(callManager_.screenShareEnabled());
}

void MainWindow::leaveCallIfActive() {
    if (!callManager_.inCall()) {
        return;
    }
    if (callManager_.videoEnabled()) {
        callManager_.disableVideo();
    }
    if (callManager_.screenShareEnabled()) {
        callManager_.disableScreenShare();
    }
    callManager_.leaveCall();
    callParticipants_.clear();
    callWindow_->setCallParticipants(callParticipants_);
    // resetForNewCall() уже возвращает плитки в videoStrip_ первым делом
    // (issue #215, на случай если звонок закончился, пока был свёрнут) —
    // оверлей на этот момент мог быть виден, прячем и его, не только
    // callWindow_.
    callWindow_->resetForNewCall();
    callWindow_->hide();
    floatingCallTilesOverlay_->hide();
    chatView_->setCallState(false);
}

void MainWindow::onEditProfileClicked() {
    profileDialog_->statusLabel()->clear();
    profileDialog_->show();
    profileDialog_->raise();
    profileDialog_->activateWindow();
}

void MainWindow::onRequestOtpCodeClicked(const QString& identifier) {
    authClient_.requestOtp(identifier);
}

void MainWindow::onVerifyOtpCodeClicked(const QString& identifier, const QString& code) {
    authClient_.verifyOtp(identifier, code);
}

void MainWindow::onAccountSettingsClicked() {
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setObjectName(QStringLiteral("accountSettingsMenu"));

    QAction* editProfileAction = menu->addAction(tr("Edit Profile..."));
    editProfileAction->setObjectName(QStringLiteral("editProfileAction"));
    editProfileAction->setEnabled(!currentUserLogin_.isEmpty());
    connect(editProfileAction, &QAction::triggered, this, &MainWindow::onEditProfileClicked);

    QAction* signOutAction = menu->addAction(tr("Sign Out"));
    signOutAction->setObjectName(QStringLiteral("signOutAction"));
    signOutAction->setEnabled(!currentUserLogin_.isEmpty());
    connect(signOutAction, &QAction::triggered, this, &MainWindow::signOut);

    menu->popup(footerBar_->avatarLabel()->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu->sizeHint().height()));
}

void MainWindow::signOut() {
    refreshTimer_->stop();
    lastToken_.clear();
    refreshToken_.clear();
    currentUserLogin_.clear();
    identityKeyStore_.reset();
    showCommunitiesMode();
    closeChatView();
    friendsPanel_->setFriends({});
    friendsPanel_->setIncomingRequests({});
    selectedCommunityId_ = -1;
    pendingCommunitySelection_ = -1;
    pendingChannelSelection_ = -1;
    channelKeys_.clear();
    pendingEncryptedSetup_.reset();
    pendingDownloadFilenames_.clear();
    communities_.clear();
    channels_.clear();
    communitiesPanel_->setCommunities(communities_);
    channelsPanel_->setChannels(channels_);
    communitiesPanel_->setCurrentUserLogin(QString());
    channelsPanel_->setCurrentUserLogin(QString());
    chatView_->setCurrentUserLogin(QString());
    footerBar_->setProfileText(tr("Not signed in"));

    // Тот же гейтинг, что и на старте (issue #156) — без токена
    // показывать интерфейс не для кого, так что он снова прячется, а
    // LoginWindow возвращается на первый шаг и показывается заново.
    hide();
    loginWindow_->reset();
    loginWindow_->show();
}

void MainWindow::refreshCommunities() {
    if (lastToken_.isEmpty()) {
        showToast(tr("Sign in first (Account menu, top right)"), ToastBanner::Variant::kInfo);
        return;
    }
    chatRestClient_.listCommunities(lastToken_);
}

void MainWindow::refreshChannelsForSelectedCommunity() {
    if (selectedCommunityId_ < 0) {
        channelsPanel_->setChannels({});
        return;
    }
    chatRestClient_.listChannels(lastToken_, selectedCommunityId_);
}

bool MainWindow::currentUserCanManageChannel() const {
    if (selectedCommunityId_ < 0 || currentUserLogin_.isEmpty()) {
        return false;
    }
    const auto it = std::find_if(communities_.cbegin(), communities_.cend(),
                                  [this](const ChatItem& item) { return item.id == selectedCommunityId_; });
    const bool isOwner = it != communities_.cend() && it->ownerLogin == currentUserLogin_;
    return isOwner || currentCommunityModeratorLogins_.contains(currentUserLogin_);
}

void MainWindow::openChannel(qint64 id, const QString& name) {
    // Звонок привязан к тому каналу, на который мы подписаны — выходим
    // из него перед переключением, а не оставляем PeerConnection
    // висящими на канале, к которому мы уже даже не подключены.
    leaveCallIfActive();
    selectedChannelId_ = id;
    oldestMessageId_ = -1;
    channelsPanel_->setOpenChannelId(id);
    chatClient_.disconnectFromChannel();
    chatView_->showChannel(name);
    chatView_->clearLog();
    // Issue #338: clearLog() выше уже сбросила это в false — выставляем
    // заново из уже (скорее всего) известной роли в сообществе,
    // запрошенной при его выборе (см. communitySelected()).
    chatView_->setCanManageChannel(currentUserCanManageChannel());
    searchDialog_->clearResults();

    const auto it = std::find_if(channels_.cbegin(), channels_.cend(), [id](const ChatItem& item) { return item.id == id; });
    currentChannelEncrypted_ = it != channels_.cend() && it->isEncrypted;
    chatView_->setEncrypted(currentChannelEncrypted_);
    memberListPanel_->setChannelEncrypted(currentChannelEncrypted_);

    if (currentChannelEncrypted_ && !channelKeys_.contains(id)) {
        // Отложено до myChannelKeyFetched()/myChannelKeyNotFound() —
        // нет смысла подписываться/загружать историю, пока не известно,
        // сможем ли мы вообще её расшифровать.
        chatRestClient_.fetchMyChannelKey(lastToken_, id);
        return;
    }
    finishOpeningChannel(id);
}

void MainWindow::finishOpeningChannel(qint64 id) {
    chatClient_.connectToChannel(lastToken_, id);
    chatRestClient_.listMessages(lastToken_, id, kMessagePageSize);
    chatRestClient_.listPinnedMessages(lastToken_, id);
}

void MainWindow::closeChatView() {
    leaveCallIfActive();
    if (selectedChannelId_ >= 0) {
        chatClient_.disconnectFromChannel();
    }
    selectedChannelId_ = -1;
    oldestMessageId_ = -1;
    currentChannelEncrypted_ = false;
    memberListPanel_->setChannelEncrypted(false);
    channelsPanel_->setOpenChannelId(-1);
    chatView_->showPlaceholder();
    searchDialog_->clearResults();
    // Issue #338 — принадлежали только что закрытому каналу.
    currentPinnedMessages_.clear();
    pinnedMessagesDialog_->setPinnedMessages({});
}

void MainWindow::wrapPendingEncryptedChannelKeyForMember(const QString& login, const QString& publicKeyBase64) {
    if (!pendingEncryptedSetup_.has_value() || !pendingEncryptedSetup_->pendingMemberLogins.contains(login)) {
        return;
    }
    pendingEncryptedSetup_->pendingMemberLogins.remove(login);

    if (publicKeyBase64.isEmpty()) {
        showToast(tr("'%1' hasn't set up encryption yet and won't have access to this channel").arg(login),
                   ToastBanner::Variant::kInfo);
    } else {
        const QString wrappedKey = channel_crypto::wrapKeyForRecipient(
            pendingEncryptedSetup_->channelKey, QByteArray::fromBase64(publicKeyBase64.toUtf8()));
        chatRestClient_.setChannelKey(lastToken_, pendingEncryptedSetup_->channelId, login, wrappedKey);
    }

    if (pendingEncryptedSetup_->pendingMemberLogins.isEmpty()) {
        pendingEncryptedSetup_.reset();
    }
}

void MainWindow::grantChannelKeyAccess(const QString& login) {
    if (!currentChannelEncrypted_ || !channelKeys_.contains(selectedChannelId_)) {
        // Нечем делиться — у самого вошедшего пользователя ключ этого
        // канала ещё не развёрнут (issue #217 требует явную ошибку,
        // а не молчаливый no-op).
        showToast(tr("You don't have this channel's key yourself yet"), ToastBanner::Variant::kError);
        return;
    }
    pendingKeyGrant_ = PendingKeyGrant{.channelId = selectedChannelId_, .targetLogin = login};
    userProfileClient_.fetchProfile(lastToken_, login);
}

void MainWindow::finishGrantingChannelKeyAccess(const QString& login, const QString& publicKeyBase64) {
    if (!pendingKeyGrant_.has_value() || pendingKeyGrant_->targetLogin != login) {
        return;  // Не связано с текущим запросом "поделиться ключом".
    }
    const PendingKeyGrant grant = *pendingKeyGrant_;
    pendingKeyGrant_.reset();

    if (publicKeyBase64.isEmpty()) {
        showToast(tr("'%1' hasn't set up encryption yet and can't be granted access").arg(login),
                   ToastBanner::Variant::kError);
        return;
    }
    const QString wrappedKey = channel_crypto::wrapKeyForRecipient(
        channelKeys_[grant.channelId], QByteArray::fromBase64(publicKeyBase64.toUtf8()));
    chatRestClient_.setChannelKey(lastToken_, grant.channelId, login, wrappedKey);
    showToast(tr("Granted '%1' access to this channel").arg(login), ToastBanner::Variant::kSuccess);
}

QString MainWindow::decryptForDisplay(const QString& ciphertext) const {
    const auto it = channelKeys_.constFind(selectedChannelId_);
    if (it == channelKeys_.constEnd()) {
        return tr("\U0001F512 [no key for this channel yet]");
    }
    return channel_crypto::decryptMessage(ciphertext, it.value()).value_or(tr("\U0001F512 [unable to decrypt]"));
}

void MainWindow::showToast(const QString& text, ToastBanner::Variant variant) {
    toastBanner_->showMessage(text, variant, kToastTimeoutMs);
}

void MainWindow::onFriendsButtonClicked() {
    if (friendsPanel_->isOpen()) {
        showCommunitiesMode();
    } else {
        showFriendsMode();
    }
}

void MainWindow::showFriendsMode() {
    friendsPanel_->setOpen(true);
    contentStack_->setCurrentWidget(directMessageView_);
    // Список участников — для канала сообщества, в режиме друзей нет
    // выбранного сообщества, которое он мог бы описывать (issue #182 +
    // #187 — панель и режим друзей появились в двух независимых PR, не
    // знавших друг о друге).
    memberListPanel_->setVisible(false);
    directMessageView_->showPlaceholder();
    openDmThreadId_ = -1;
    openDmOtherLogin_.clear();
    dmChatClient_.disconnectFromChannel();
    if (!lastToken_.isEmpty()) {
        userProfileClient_.listFriends(lastToken_);
        userProfileClient_.listIncomingFriendRequests(lastToken_);
    }
}

void MainWindow::showCommunitiesMode() {
    friendsPanel_->setOpen(false);
    contentStack_->setCurrentWidget(chatView_);
    memberListPanel_->setVisible(true);
    openDmThreadId_ = -1;
    openDmOtherLogin_.clear();
    dmChatClient_.disconnectFromChannel();
}

void MainWindow::openDmThreadWith(const QString& login) {
    if (lastToken_.isEmpty()) {
        showToast(tr("Sign in first (Account menu, top right)"), ToastBanner::Variant::kInfo);
        return;
    }
    chatRestClient_.openDmThread(lastToken_, login);
}

}  // namespace devicehub
