#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QSet>
#include <memory>
#include <optional>

#include "auth/AuthClient.h"
#include "chat/CallManager.h"
#include "chat/ChatClient.h"
#include "chat/ChatRestClient.h"
#include "devices/AudioInputDevice.h"
#include "devices/AudioOutputDevice.h"
#include "devices/CameraDevice.h"
#include "devices/DeviceEnumerator.h"
#include "devices/ScreenCaptureDevice.h"
#include "devices/VoiceMessageRecorder.h"
#include "ui/ToastBanner.h"
#include "user/IdentityKeyStore.h"
#include "user/UserProfileClient.h"

class QScreen;
class QStackedWidget;
class QTimer;

namespace devicehub {

class ChannelsPanel;
class CallWindow;
class ChatView;
class CommunitiesPanel;
class DesktopNotifier;
class DirectMessageView;
class FloatingCallTilesOverlay;
class FooterBar;
class FriendsPanel;
class LoginWindow;
class MemberListPanel;
class ModeratorsDialog;
class ProfileDialog;
class SearchDialog;
class PinnedMessagesDialog;
class SettingsDialog;

/**
 * @brief Оболочка главного окна: боковая панель сообществ/каналов
 *        слева, чат открытого канала в основной области, подвал с
 *        профилем и точкой входа в настройки.
 *
 * Интерфейс скрыт, пока пользователь не авторизован — единственное
 * видимое окно при запуске это LoginWindow (issue #156); само
 * MainWindow показывается только после успешного входа/регистрации, а
 * закрытие LoginWindow без авторизации завершает приложение (см.
 * main.cpp и обработчик LoginWindow::rejected() в конструкторе).
 *
 * Чистое представление/связующая логика — весь доступ к устройствам и
 * сети делегирован классам devicehub::* в src/devices, src/auth и
 * src/chat; всё конструирование виджетов делегировано классам панелей
 * в src/ui.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    void populateDevices();
    void onPlayToneClicked();
    void onToggleMicClicked();
    void onToggleCameraClicked();
    void onToggleScreenCaptureClicked();
    void onSendChatMessageClicked();
    /// Клик по "Attach" (issue #116) — открывает выбор файла, затем
    /// загружает выбранный файл; сама отправка происходит после того,
    /// как сработает ChatRestClient::attachmentUploaded() (см.
    /// MainWindow.cpp).
    void onAttachFileClicked();
    /// Клик по кнопке записи голосового сообщения (issue #359) —
    /// переключает voiceMessageRecorder_ старт/стоп; на стоп сразу
    /// загружает готовый WAV через ChatRestClient::uploadAttachment(),
    /// тем же путём, что и обычное файловое вложение (см. подключение
    /// ChatRestClient::attachmentUploaded() в MainWindow.cpp — оно уже
    /// отправляет сообщение с этим attachment_id, отдельно вызывать
    /// sendMessage() здесь не нужно).
    void onRecordVoiceToggleClicked();
    void onCallToggleClicked();
    void onMuteToggleClicked();
    void onVideoToggleClicked();
    void onEditProfileClicked();
    void onScreenShareToggleClicked();
    /// CallWindow::minimizeRequested() (клик "Minimize" либо закрытие
    /// самого окна, issue #215) — переносит текущие видео-плитки в
    /// floatingCallTilesOverlay_ и показывает его вместо CallWindow.
    void onCallMinimizeRequested();
    /// FloatingCallTilesOverlay::restoreRequested() (issue #215) —
    /// обратное действие: возвращает плитки в CallWindow и показывает
    /// его снова вместо оверлея.
    void onCallRestoreRequested();

    /// Выходит из текущего звонка, если он вообще идёт — общая часть
    /// onCallToggleClicked()/openChannel()/closeChatView() (звонок
    /// привязан к каналу, поэтому уходит вместе с ним при
    /// переключении/закрытии), включая скрытие callWindow_.
    void leaveCallIfActive();
    /// LoginWindow::requestCodeRequested() — issue #156.
    void onRequestOtpCodeClicked(const QString& identifier);
    /// LoginWindow::verifyCodeRequested() — issue #156.
    void onVerifyOtpCodeClicked(const QString& identifier, const QString& code);
    /// LoginWindow::passwordSignInRequested() — issue #156.
    void onPasswordSignInClicked(const QString& login, const QString& password);
    /// LoginWindow::registerRequested() — issue #156.
    void onRegisterClicked(const QString& login, const QString& password);
    /// Клик по аватару в футере (issue #151) — показывает небольшое меню
    /// (Edit Profile / Sign Out), привязанное к аватару.
    void onAccountSettingsClicked();
    /// Очищает локальное состояние авторизации и возвращает UI в
    /// состояние "не авторизован" — эндпоинта отзыва токена на сервере
    /// пока не существует, поэтому это выход только на стороне клиента.
    void signOut();

    /// Заново запрашивает список сообществ у chat-service (ничего не
    /// делает, кроме сообщения в статус-баре, если вход ещё не
    /// выполнен).
    void refreshCommunities();
    /// Заново запрашивает список каналов для selectedCommunityId_
    /// (ничего не делает, кроме сообщения в статус-баре, если сообщество
    /// не выбрано).
    void refreshChannelsForSelectedCommunity();
    /// True, если вошедший пользователь — владелец selectedCommunityId_
    /// (по communities_) или входит в currentCommunityModeratorLogins_
    /// (issue #338) — пересчитывается по требованию, не кэшируется
    /// отдельно, поскольку оба входа (communities_/
    /// currentCommunityModeratorLogins_) уже сами кэшированы и меняются
    /// нечасто.
    [[nodiscard]] bool currentUserCanManageChannel() const;
    /// Переключает ChatView на @p id/@p name, (пере)подключая
    /// ChatClient. Для зашифрованного канала (issue #138), для которого
    /// ключ ещё не закэширован, сначала запрашивает/разворачивает ключ
    /// и откладывает подписку/загрузку истории до
    /// finishOpeningChannel(), чтобы ничто не пыталось расшифровывать
    /// до того, как ключ станет доступен.
    void openChannel(qint64 id, const QString& name);
    /// Вторая половина openChannel() — подписывает ChatClient и
    /// загружает историю. Вызывается сразу для незашифрованного канала
    /// или канала, чей ключ уже закэширован; иначе вызывается из
    /// обработчиков myChannelKeyFetched()/myChannelKeyNotFound().
    void finishOpeningChannel(qint64 id);
    /// Сбрасывает текущий выбор/подключение канала и снова показывает
    /// заглушку ChatView.
    void closeChatView();
    /// Один шаг отложенного сценария создания зашифрованного канала
    /// (issue #138): если @p login есть в
    /// pendingEncryptedSetup_->pendingMemberLogins, оборачивает
    /// отложенный ключ канала для @p publicKeyBase64 (если он не пуст —
    /// пустой означает, что этот участник ещё не опубликовал ключ,
    /// поэтому он пропускается с показом toast) и публикует его через
    /// setChannelKey(). Ничего не делает, если @p login сейчас не
    /// ожидается (то есть это не связанный с этим profileReceived(),
    /// например, собственный профиль вошедшего пользователя).
    void wrapPendingEncryptedChannelKeyForMember(const QString& login, const QString& publicKeyBase64);
    /// Обработчик MemberListPanel::grantChannelKeyAccessRequested()
    /// (issue #217) — "поделиться ключом" открытого сейчас
    /// зашифрованного канала с конкретным @p login. Явная ошибка
    /// тостом, если у самого вошедшего пользователя ещё нет
    /// собственного ключа этого канала (нечем делиться), иначе
    /// запрашивает открытый ключ @p login через fetchProfile() —
    /// заворачивание и публикация завершаются в
    /// finishGrantingChannelKeyAccess() по ответу profileReceived().
    void grantChannelKeyAccess(const QString& login);
    /// Вторая половина grantChannelKeyAccess() — вызывается из
    /// обработчика profileReceived() для любого профиля, а не только
    /// связанного с этим запросом, поэтому сверяет @p login с
    /// pendingKeyGrant_ и ничего не делает, если это не он. Явная ошибка
    /// тостом (issue #217), если @p publicKeyBase64 пуст — целевой
    /// участник ещё не опубликовал открытый ключ, значит поделиться с
    /// ним нечем.
    void finishGrantingChannelKeyAccess(const QString& login, const QString& publicKeyBase64);
    /// Расшифровывает @p ciphertext ключом channelKeys_[selectedChannelId_]
    /// для отображения — строка-заглушка (никогда не исходный
    /// шифротекст), если ключ ещё не закэширован или расшифровка не
    /// удалась, так что сбой расшифровки читается как "не удаётся
    /// расшифровать", а не показывает нечитаемые байты.
    [[nodiscard]] QString decryptForDisplay(const QString& ciphertext) const;
    /// Обратная связь по CRUD-операциям (создание/переименование/
    /// удаление/присоединение, ошибки) идёт через этот toast, а не
    /// через statusBar() — так гораздо легче заметить.
    void showToast(const QString& text, ToastBanner::Variant variant);

    /// Открывает FriendsPanel как всплывающую панель поверх ChannelsPanel
    /// (issue #187, Фаза 3; issue #216 — оверлей вместо подмены панели в
    /// общей раскладке) и переключает основную область на
    /// DirectMessageView вместо ChatView; заново запрашивает список
    /// друзей и входящих заявок.
    void showFriendsMode();
    /// Обратное переключение — сворачивает FriendsPanel и возвращает
    /// основную область к ChatView. Вызывается и по повторному клику на
    /// кнопку "Friends" (см. onFriendsButtonClicked()), и при выборе
    /// сообщества, и при выходе из аккаунта — тем самым не нужно
    /// отдельной кнопки "назад" в самой FriendsPanel.
    void showCommunitiesMode();
    /// Кнопка "Friends" в CommunitiesPanel — переключатель (issue #216):
    /// открывает FriendsPanel, если она сейчас свёрнута, иначе сворачивает.
    void onFriendsButtonClicked();
    /// Открывает диалог с @p login — вызывается по клику на друга в
    /// FriendsPanel; фактическое переключение contentStack_ происходит
    /// в обработчике ChatRestClient::dmThreadOpened(), а не здесь,
    /// поскольку id диалога до ответа сервера ещё не известен.
    void openDmThreadWith(const QString& login);

    DeviceEnumerator enumerator_;
    AudioOutputDevice audioOutput_;
    AudioInputDevice audioInput_;
    CameraDevice camera_;
    ScreenCaptureDevice screenCapture_;
    /// Issue #359 — своя запись, независимая от audioInput_ выше (та —
    /// общая для mic-теста в настройках и CallManager во время звонка).
    VoiceMessageRecorder voiceMessageRecorder_;
    AuthClient authClient_;
    ChatClient chatClient_;
    /// Отдельное WebSocket-соединение для живой доставки личных
    /// сообщений (issue #187, Фаза 2b) — не то же самое, что chatClient_
    /// (подписан на канал сообщества и используется CallManager для
    /// сигналинга звонка); диалог ЛС не должен занимать эту подписку.
    ChatClient dmChatClient_;
    CallManager callManager_{chatClient_, audioInput_, audioOutput_, camera_, screenCapture_};
    ChatRestClient chatRestClient_;
    UserProfileClient userProfileClient_;
    /// Конструируется, как только становится известен currentUserLogin_
    /// (issue #136) — без конструктора по умолчанию, поскольку пара
    /// ключей бессмысленна без логина, к которому привязывается файл
    /// её хранения.
    std::optional<IdentityKeyStore> identityKeyStore_;
    QString lastToken_;
    /// Долгоживущий токен (issue #105), который refreshTimer_
    /// обменивает на свежий lastToken_ незадолго до истечения срока
    /// действия — пуст, когда вход не выполнен.
    QString refreshToken_;
    QTimer* refreshTimer_ = nullptr;
    /// Issue #310/#349 — периодически (раз в 30с), плюс сразу при входе
    /// и при переключении сообщества, вызывает
    /// ChatRestClient::fetchUnreadCounts() для обновления бейджей
    /// непрочитанного в CommunitiesPanel/ChannelsPanel. Отдельный от
    /// refreshTimer_ таймер — тот обменивает refresh-токен один раз
    /// незадолго до истечения срока действия, не по регулярному циклу.
    QTimer* unreadPollTimer_ = nullptr;
    QString currentUserLogin_;
    QList<QScreen*> screens_;
    QList<ChatItem> communities_;
    QList<ChatItem> channels_;
    /// Issue #310/#349 — накапливается по мере того, как
    /// channelsListed() возвращает канал для того сообщества, что было
    /// выбрано на момент запроса (см. doc-комментарий в .cpp у
    /// обработчика channelsListed) — используется, чтобы просуммировать
    /// бейджи непрочитанного по каналам в один бейдж на сообщество.
    /// Сообщество, которое пользователь ни разу не открывал в этой
    /// сессии, здесь не появится — его бейдж останется неизвестным до
    /// первого открытия (не отслеживается как отдельная задача — тот же
    /// класс компромисса, что и REST-refetch вместо live push, см.
    /// README).
    QHash<qint64, qint64> channelIdToCommunityId_;
    /// Модераторы сообщества, которому принадлежит открытый канал
    /// (issue #338) — обновляется при выборе сообщества
    /// (ChatRestClient::listModerators()), используется вместе с
    /// communities_[...].ownerLogin, чтобы решить, показывать ли
    /// Pin/Unpin (см. ChatView::setCanManageChannel()). Отдельно от
    /// moderatorsDialog_, который показывает тот же список только
    /// владельцу для управления ролями.
    QStringList currentCommunityModeratorLogins_;
    /// Закреплённые сообщения открытого канала (issue #338) — источник
    /// для pinnedMessagesDialog_ и счётчика на pinnedMessagesButton();
    /// обновляется целиком по ChatRestClient::listPinnedMessages() и
    /// точечно по ChatClient::messagePinned()/messageUnpinned().
    QList<PinnedMessageInfo> currentPinnedMessages_;
    QStringList callParticipants_;
    qint64 selectedCommunityId_ = -1;
    qint64 selectedChannelId_ = -1;
    qint64 pendingCommunitySelection_ = -1;
    qint64 pendingChannelSelection_ = -1;
    /// Id самого старого сообщения, которое ChatView сейчас показывает
    /// для открытого канала, или -1, если ещё ничего не загружено —
    /// курсор beforeId для следующей загрузки "Load older messages".
    /// Сбрасывается при каждом переключении канала.
    qint64 oldestMessageId_ = -1;
    /// Имя файла, предлагаемое в диалоге сохранения, когда придёт
    /// соответствующий ответ ChatRestClient::attachmentDownloaded()
    /// (issue #116) — ключом служит id вложения, поскольку загрузки
    /// могут выполняться одновременно для нескольких сообщений.
    QHash<qint64, QString> pendingDownloadFilenames_;

    /// Разрешённые (развёрнутые) исходные симметричные ключи для
    /// зашифрованных каналов (issue #138), ключ — id канала — живут
    /// только в рамках сессии, никогда не сохраняются на диск.
    /// Отсутствие записи для зашифрованного канала означает либо что
    /// ключ ещё не запрошен/развёрнут, либо что для этого логина он не
    /// был обёрнут (см. myChannelKeyNotFound()).
    QHash<qint64, QByteArray> channelKeys_;
    /// True, пока selectedChannelId_ указывает на зашифрованный канал —
    /// управляет шифрованием перед отправкой/расшифровкой перед
    /// показом и отключает Attach/Search (не поддерживаются для
    /// зашифрованных каналов на этом этапе).
    bool currentChannelEncrypted_ = false;

    /// Состояние для многошагового сценария "создать зашифрованный
    /// канал": сгенерировать ключ, затем обернуть и опубликовать его
    /// для каждого участника сообщества, кто уже опубликовал открытый
    /// ключ (issue #136). Валидно только между channelCreated() для
    /// зашифрованного канала и завершением последнего вызова
    /// setChannelKey().
    struct PendingEncryptedChannelSetup {
        qint64 channelId = -1;
        QByteArray channelKey;
        /// Логины, которые ещё ждут ответа fetchProfile(), чтобы узнать
        /// свой открытый ключ, прежде чем для них можно будет обернуть
        /// ключ канала.
        QSet<QString> pendingMemberLogins;
    };
    std::optional<PendingEncryptedChannelSetup> pendingEncryptedSetup_;

    /// Состояние однократного сценария "поделиться ключом канала с
    /// конкретным участником" (issue #217) — валидно между вызовом
    /// grantChannelKeyAccess() и завершением ответного
    /// finishGrantingChannelKeyAccess() для того же логина.
    struct PendingKeyGrant {
        qint64 channelId = -1;
        QString targetLogin;
    };
    std::optional<PendingKeyGrant> pendingKeyGrant_;

    /// Id открытого сейчас диалога личных сообщений (issue #187, Фаза
    /// 3), -1 — ни один не открыт (режим Friends ещё не активен либо
    /// друг ещё не выбран).
    qint64 openDmThreadId_ = -1;
    QString openDmOtherLogin_;

    CommunitiesPanel* communitiesPanel_ = nullptr;
    ChannelsPanel* channelsPanel_ = nullptr;
    /// Всплывает поверх ChannelsPanel, не заменяет её в раскладке (issue
    /// #216) — см. doc-комментарий класса FriendsPanel.
    FriendsPanel* friendsPanel_ = nullptr;
    ChatView* chatView_ = nullptr;
    CallWindow* callWindow_ = nullptr;
    /// Показывается вместо callWindow_, пока звонок свёрнут (issue
    /// #215) — см. doc-комментарий CallWindow о detachTilesTo()/
    /// reattachTiles().
    FloatingCallTilesOverlay* floatingCallTilesOverlay_ = nullptr;
    MemberListPanel* memberListPanel_ = nullptr;
    DirectMessageView* directMessageView_ = nullptr;
    QStackedWidget* contentStack_ = nullptr;
    FooterBar* footerBar_ = nullptr;
    SettingsDialog* settingsDialog_ = nullptr;
    ModeratorsDialog* moderatorsDialog_ = nullptr;
    ProfileDialog* profileDialog_ = nullptr;
    SearchDialog* searchDialog_ = nullptr;
    PinnedMessagesDialog* pinnedMessagesDialog_ = nullptr;
    LoginWindow* loginWindow_ = nullptr;
    ToastBanner* toastBanner_ = nullptr;
    DesktopNotifier* desktopNotifier_ = nullptr;
};

}  // namespace devicehub
