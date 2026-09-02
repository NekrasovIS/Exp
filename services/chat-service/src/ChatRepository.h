#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chat_service {

struct Community {
    std::int64_t id = 0;
    std::string name;
    std::string ownerLogin;
};

struct Channel {
    std::int64_t id = 0;
    std::int64_t communityId = 0;
    std::string name;
    std::string ownerLogin;
    /// E2E-шифрование, фаза 2 (issue #138) — устанавливается только при
    /// создании, никогда не меняется впоследствии (нет пути из открытой
    /// истории в зашифрованную или обратно). Когда true, chat-service
    /// хранит только шифротекст тел сообщений и обёрнутые копии
    /// симметричного ключа канала для каждого участника (см.
    /// setChannelKey()/findChannelKey()) — он никогда не видит сырой
    /// ключ или открытый текст тела.
    bool isEncrypted = false;
};

/// Результат попытки переименования/удаления — различает "не существует"
/// и "существует, но вы им не владеете", чтобы HTTP-обработчики могли
/// ответить правильным кодом статуса (404 против 403), а не сворачивать
/// оба случая в единую обобщённую неудачу.
enum class MutationResult {
    kSuccess,
    kNotFound,
    kForbidden,
    kConflict,  // например, переименование канала в имя, уже занятое в том же сообществе
};

struct Message {
    std::int64_t id = 0;
    std::string authorLogin;
    std::string body;
    std::string sentAt;                   // ISO 8601, как возвращает Postgres
    std::optional<std::string> editedAt;  // не установлено, если сообщение никогда не редактировалось (issue #107)
    std::optional<std::int64_t> attachmentId;    // не установлено для обычного текстового сообщения (issue #116)
    std::optional<std::string> attachmentFilename;  // установлено тогда и только тогда, когда установлен attachmentId
};

/// Метаданные о сохранённом вложении (issue #116) — всё, кроме сырых
/// байтов, которые findAttachmentData() получает отдельно, чтобы
/// маршрут, которому нужны только метаданные (например, отрисовка
/// сообщения), не тянул мегабайты данных через Postgres впустую.
struct AttachmentMetadata {
    std::int64_t id = 0;
    std::string filename;
    std::string contentType;
    std::int64_t sizeBytes = 0;
};

/// Данные сохранённого вложения для GET /attachments/{id} — @p data
/// всё ещё в base64 (как и хранится; см. doc-комментарий ChatRepository
/// о причинах), поэтому HttpServer декодирует их перед записью тела
/// HTTP-ответа.
struct AttachmentData {
    std::string filename;
    std::string contentType;
    std::string data;
};

/// Поля, нужные createAttachment() помимо целевого channelId —
/// сгруппированы по правилу CLAUDE.md "предпочитать меньше аргументов
/// функции", а не 6-параметровая сигнатура createAttachment().
struct AttachmentUpload {
    std::string uploaderLogin;
    std::string filename;
    std::string contentType;
    /// Уже закодировано в base64 вызывающей стороной — см. doc-комментарий
    /// этого класса о том, почему вложения хранятся как base64 TEXT.
    std::string dataBase64;
    /// Количество байт *после декодирования* (HttpServer вычисляет его,
    /// чтобы применить ограничение размера до вызова createAttachment();
    /// сохраняется, чтобы вызывающим findAttachmentData() не нужно было
    /// декодировать только ради того, чтобы узнать размер).
    std::int64_t sizeBytes = 0;
};

/// Результат editMessage() — одного MutationResult недостаточно, чтобы
/// передать новое значение edited_at, присвоенное Postgres, которое
/// нужно WebSocketServer для payload'а рассылки.
struct EditMessageResult {
    MutationResult result = MutationResult::kNotFound;
    std::string editedAt;  // имеет смысл только при result == kSuccess
};

/**
 * @brief Хранилище на базе Postgres для сообществ/каналов/членств/
 *        сообщений/вложений.
 *
 * Открывает новое соединение на каждый вызов вместо пулинга —
 * pqxx::connection не потокобезопасен, а HttpServer/WebSocketServer
 * могут диспетчеризовать из нескольких потоков; см. UserRepository в
 * user-service — тот же компромисс и то же обоснование.
 *
 * Роли (issue #114): у каждого сообщества ровно один владелец
 * (communities.owner_login, устанавливается при создании, никогда не
 * передаётся) плюс произвольное число модераторов
 * (memberships.is_moderator), которых владелец назначает/снимает.
 * Модератор может удалить любое сообщение и переименовать/удалить любой
 * канал в пределах этого одного сообщества — но никогда не трогает само
 * сообщество (переименование/удаление сообщества, назначение/снятие
 * других модераторов) и никогда не редактирует чужое сообщение (см.
 * doc-комментарий editMessage()). Статус модератора не переносится между
 * сообществами: будучи модератором одного, в другом это не даёт ничего.
 *
 * Вложения (issue #116) хранятся как строки TEXT в base64
 * непосредственно в этой же базе Postgres, а не как сырой BYTEA — это
 * обходит привязку бинарных параметров libpqxx (pqxx::blob нацелен на
 * Large Objects, механизм, отличный от колонки BYTEA) ценой обычных
 * ~33% накладных расходов base64 по размеру — и вместо диска или
 * объектного хранилища это простейший вариант для первой версии, с
 * ограничением размера (см. kMaxAttachmentSizeBytes в HttpServer.cpp),
 * а не рассчитанный на масштабирование до больших файлов или реальной
 * продакшн-нагрузки.
 */
class ChatRepository {
public:
    explicit ChatRepository(std::string connectionString);

    [[nodiscard]] Community createCommunity(const std::string& name, const std::string& ownerLogin);
    [[nodiscard]] std::vector<Community> listCommunities();
    [[nodiscard]] MutationResult renameCommunity(std::int64_t id, const std::string& newName,
                                                  const std::string& requesterLogin);
    [[nodiscard]] MutationResult deleteCommunity(std::int64_t id, const std::string& requesterLogin);

    /// @return Id нового канала, либо std::nullopt, если @p communityId не существует
    ///         или уже имеет канал с именем @p name.
    [[nodiscard]] std::optional<std::int64_t> createChannel(std::int64_t communityId, const std::string& name,
                                                             const std::string& ownerLogin,
                                                             bool isEncrypted = false);
    [[nodiscard]] std::vector<Channel> listChannels(std::int64_t communityId);

    /// @return Канал с идентификатором @p id, либо std::nullopt, если его
    ///         не существует — аналог listChannels() для одного канала
    ///         (которому нужен id родительского сообщества, а не id
    ///         канала), например для проверки isEncrypted перед запросом
    ///         поиска/вложения.
    [[nodiscard]] std::optional<Channel> findChannel(std::int64_t id);

    /// Логины всех текущих участников @p communityId в произвольном
    /// порядке — тот же стиль "просто вернуть ничего", что и у
    /// listModerators() для несуществующего сообщества. Нужно, чтобы
    /// знать, для кого обернуть ключ зашифрованного канала в момент его
    /// создания (issue #138).
    [[nodiscard]] std::vector<std::string> listMembers(std::int64_t communityId);

    /// Разрешено собственному владельцу канала, владельцу родительского
    /// сообщества либо модератору родительского сообщества (issue #114)
    /// — а не только тому, кто случайно создал именно этот канал.
    [[nodiscard]] MutationResult renameChannel(std::int64_t id, const std::string& newName,
                                                const std::string& requesterLogin);

    /// То же правило полномочий, что и у renameChannel().
    [[nodiscard]] MutationResult deleteChannel(std::int64_t id, const std::string& requesterLogin);

    /// Идемпотентно: вступление в сообщество, в котором вы уже состоите, успешно.
    /// @return False, если @p communityId не существует.
    [[nodiscard]] bool joinCommunity(std::int64_t communityId, const std::string& login);

    /// Назначает @p targetLogin модератором @p communityId — только для
    /// владельца (issue #114). Неявно добавляет @p targetLogin в
    /// сообщество первым делом, если он ещё не был участником (тот же
    /// эффект, что и joinCommunity()) — модератор, который даже не
    /// является участником, был бы запутанным состоянием.
    [[nodiscard]] MutationResult promoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                   const std::string& requesterLogin);

    /// Только для владельца. Снятие с должности того, кто никогда не был
    /// модератором (или даже не участник) — безобидная успешная операция
    /// без эффекта, а не ошибка — тот же идемпотентный стиль, что и у
    /// joinCommunity().
    [[nodiscard]] MutationResult demoteModerator(std::int64_t communityId, const std::string& targetLogin,
                                                  const std::string& requesterLogin);

    /// Логины всех текущих модераторов @p communityId в произвольном
    /// порядке — пусто (не ошибка) для несуществующего сообщества, тот
    /// же стиль "просто вернуть ничего", что и у
    /// listChannels()/listCommunities().
    [[nodiscard]] std::vector<std::string> listModerators(std::int64_t communityId);

    /// @return Сохранённое сообщение (с присвоенными id/временной меткой),
    ///         либо std::nullopt, если @p channelId не существует, либо
    ///         @p attachmentId установлен, но не существует/принадлежит
    ///         другому каналу.
    [[nodiscard]] std::optional<Message> insertMessage(std::int64_t channelId, const std::string& authorLogin,
                                                        const std::string& body,
                                                        std::optional<std::int64_t> attachmentId = std::nullopt);

    /// Сохраняет @p upload дословно как новое вложение для @p channelId —
    /// см. doc-комментарий AttachmentUpload по её полям.
    /// @return std::nullopt, если @p channelId не существует.
    [[nodiscard]] std::optional<AttachmentMetadata> createAttachment(std::int64_t channelId,
                                                                       const AttachmentUpload& upload);

    /// @return Данные вложения (всё ещё в base64, как и хранятся — см.
    ///         doc-комментарий этого класса), либо std::nullopt, если
    ///         такого вложения не существует.
    [[nodiscard]] std::optional<AttachmentData> findAttachmentData(std::int64_t attachmentId);
    /// Хронологическая (от старых к новым) страница до @p limit
    /// сообщений. Если @p beforeId не установлен, страница заканчивается
    /// самым новым сообщением в канале; если установлен, страница
    /// заканчивается непосредственно перед сообщением с этим id — даёт
    /// вызывающей стороне возможность листать историю назад, передавая
    /// id самого старого уже полученного сообщения.
    [[nodiscard]] std::vector<Message> listRecentMessages(std::int64_t channelId, int limit,
                                                            std::optional<std::int64_t> beforeId = std::nullopt);

    /// Редактировать сообщение может только сам его автор (@p
    /// requesterLogin) (иначе kForbidden) — не владелец канала/сообщества
    /// и не модератор тоже, даже после issue #114: редактирование чужого
    /// сообщения позволило бы модератору вложить в чужие уста слова, что
    /// является существенно другим (и худшим) полномочием, чем прямое
    /// удаление контента. См. doc-комментарий класса WebSocketServer о
    /// различии модерации и авторства, которое здесь намеренно не
    /// размывается.
    [[nodiscard]] EditMessageResult editMessage(std::int64_t messageId, std::int64_t channelId,
                                                 const std::string& requesterLogin, const std::string& newBody);

    /// В отличие от editMessage(), удаление ДОСТУПНО владельцу
    /// родительского канала/сообщества или модератору сообщества
    /// (issue #114) — удаление плохого контента является обычной
    /// модерацией; переписывание его — нет.
    [[nodiscard]] MutationResult deleteMessage(std::int64_t messageId, std::int64_t channelId,
                                                const std::string& requesterLogin);

    /// Регистронезависимый поиск подстроки по телам сообщений
    /// @p channelId (issue #118), сначала самые новые совпадения,
    /// ограничено @p limit — простой `ILIKE '%query%'`, не полнотекстовый
    /// поиск (без ранжирования/стемминга), намеренное упрощение первой
    /// версии. Пустой результат для несуществующего канала, как и у
    /// listRecentMessages().
    [[nodiscard]] std::vector<Message> searchMessages(std::int64_t channelId, const std::string& query, int limit);

    /// Устанавливает (или перезаписывает) обёрнутую копию симметричного
    /// ключа @p channelId для @p memberLogin (issue #138) — @p wrappedKey
    /// это публичный ключ @p memberLogin, запечатывающий сырой ключ
    /// канала (libsodium crypto_box_seal), вычисляется целиком на
    /// стороне клиента; этот вызов никогда не видит сырой ключ. Те же
    /// полномочия, что и у renameChannel()/deleteChannel() — собственный
    /// владелец канала, владелец родительского сообщества или его
    /// модератор — поскольку только тот, кто уже владеет сырым ключом
    /// (создатель или кто-то, для кого он был обёрнут), в принципе мог
    /// изготовить валидную обёрнутую копию для кого-то ещё.
    [[nodiscard]] MutationResult setChannelKey(std::int64_t channelId, const std::string& memberLogin,
                                                const std::string& requesterLogin, const std::string& wrappedKey);

    /// @return Собственная обёрнутая копия ключа @p channelId для @p login,
    ///         либо std::nullopt, если для него ещё ничего не установлено
    ///         (у него нет доступа к содержимому этого зашифрованного
    ///         канала). Здесь нет проверки полномочий сверх того, что
    ///         вызывающая сторона всегда передаёт в @p login только
    ///         субъект собственного токена (обеспечивается HttpServer,
    ///         тот же паттерн "всегда собственный логин из токена", что
    ///         и у PATCH /users/me в user-service).
    [[nodiscard]] std::optional<std::string> findChannelKey(std::int64_t channelId, const std::string& login);

private:
    std::string connectionString_;
};

}  // namespace chat_service
