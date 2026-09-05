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
#include "ui/ToastBanner.h"
#include "user/IdentityKeyStore.h"
#include "user/UserProfileClient.h"

class QScreen;
class QStackedWidget;
class QTimer;

namespace devicehub {

class AccountMenu;
class ChannelsPanel;
class ChatView;
class CommunitiesPanel;
class DesktopNotifier;
class DirectMessageView;
class FooterBar;
class FriendsPanel;
class LoginWindow;
class ModeratorsDialog;
class ProfileDialog;
class SearchDialog;
class SettingsDialog;

/**
 * @brief Оболочка главного окна: боковая панель сообществ/каналов
 *        слева, чат открытого канала в основной области, меню аккаунта
 *        справа сверху и подвал с профилем и точкой входа в настройки.
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
    void onRequestTokenClicked();
    void onRegisterClicked();
    void onSendChatMessageClicked();
    /// Клик по "Attach" (issue #116) — открывает выбор файла, затем
    /// загружает выбранный файл; сама отправка происходит после того,
    /// как сработает ChatRestClient::attachmentUploaded() (см.
    /// MainWindow.cpp).
    void onAttachFileClicked();
    void onCallToggleClicked();
    void onMuteToggleClicked();
    void onVideoToggleClicked();
    void onEditProfileClicked();
    void onScreenShareToggleClicked();
    /// LoginWindow::requestCodeRequested() — issue #156.
    void onRequestOtpCodeClicked(const QString& identifier);
    /// LoginWindow::verifyCodeRequested() — issue #156.
    void onVerifyOtpCodeClicked(const QString& identifier, const QString& code);
    /// Клик по аватару в футере (issue #151) — показывает небольшое меню
    /// (Edit Profile / Sign Out), привязанное к аватару, а не только к
    /// действиям аккаунта из всплывающего AccountMenu в правом верхнем
    /// углу.
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

    /// Переключает боковую панель/основную область в режим "Friends"
    /// (issue #187, Фаза 3) — FriendsPanel вместо ChannelsPanel,
    /// DirectMessageView вместо ChatView; заново запрашивает список
    /// друзей и входящих заявок.
    void showFriendsMode();
    /// Обратное переключение — вызывается при выборе сообщества, тем
    /// самым не нужно отдельной кнопки "назад".
    void showCommunitiesMode();
    /// Открывает диалог с @p login — вызывается по клику на друга в
    /// FriendsPanel; фактическое переключение contentStack_ происходит
    /// в обработчике ChatRestClient::dmThreadOpened(), а не здесь,
    /// поскольку id диалога до ответа сервера ещё не известен.
    void openDmThreadWith(const QString& login);
    /// Тик dmPollTimer_ (issue #187, Фаза 2 backend'а пока не
    /// поддерживает живую доставку через WebSocket) — просто
    /// перезапрашивает последние сообщения открытого диалога;
    /// обработчик directMessagesListed() сам решает, какие из них уже
    /// показаны (см. dmHistoryLoaded_/lastSeenDmMessageId_).
    void pollOpenDmThread();

    DeviceEnumerator enumerator_;
    AudioOutputDevice audioOutput_;
    AudioInputDevice audioInput_;
    CameraDevice camera_;
    ScreenCaptureDevice screenCapture_;
    AuthClient authClient_;
    ChatClient chatClient_;
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
    QString currentUserLogin_;
    QList<QScreen*> screens_;
    QList<ChatItem> communities_;
    QList<ChatItem> channels_;
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

    /// Id открытого сейчас диалога личных сообщений (issue #187, Фаза
    /// 3), -1 — ни один не открыт (режим Friends ещё не активен либо
    /// друг ещё не выбран).
    qint64 openDmThreadId_ = -1;
    QString openDmOtherLogin_;
    /// False сразу после openDmThreadWith() — следующий
    /// directMessagesListed() для этого диалога заменяет весь список
    /// (setMessages()) и переключается в true; последующие вызовы (от
    /// dmPollTimer_) вместо этого только дозаписывают сообщения новее
    /// lastSeenDmMessageId_ (appendMessage()) — REST отдаёт только
    /// постраничную историю назад (before_id), не "новее X", поэтому
    /// поллинг просто перезапрашивает последние сообщения целиком и
    /// сам решает, что из них уже показано.
    bool dmHistoryLoaded_ = false;
    qint64 lastSeenDmMessageId_ = -1;
    QTimer* dmPollTimer_ = nullptr;

    CommunitiesPanel* communitiesPanel_ = nullptr;
    ChannelsPanel* channelsPanel_ = nullptr;
    FriendsPanel* friendsPanel_ = nullptr;
    QStackedWidget* sidebarListStack_ = nullptr;
    ChatView* chatView_ = nullptr;
    DirectMessageView* directMessageView_ = nullptr;
    QStackedWidget* contentStack_ = nullptr;
    AccountMenu* accountMenu_ = nullptr;
    FooterBar* footerBar_ = nullptr;
    SettingsDialog* settingsDialog_ = nullptr;
    ModeratorsDialog* moderatorsDialog_ = nullptr;
    ProfileDialog* profileDialog_ = nullptr;
    SearchDialog* searchDialog_ = nullptr;
    LoginWindow* loginWindow_ = nullptr;
    ToastBanner* toastBanner_ = nullptr;
    DesktopNotifier* desktopNotifier_ = nullptr;
};

}  // namespace devicehub
