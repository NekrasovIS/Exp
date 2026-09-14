#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>

#include <optional>

namespace devicehub {

class UserProfileClient;

/**
 * @brief Кэш реальных изображений аватара по логину (issue #442) —
 *        единственная точка, знающая про UserProfileClient::
 *        fetchAvatar()/avatarFetched()/avatarFetchFailed(); FooterBar/
 *        MemberListPanel/ChatMessageRow/ProfileDialog вызывают только
 *        imageFor(), сам сетевой клиент не трогают, и делят один и тот
 *        же экземпляр (владеет MainWindow) — так один и тот же login
 *        запрашивается по сети не больше одного раза одновременно,
 *        независимо от того, сколько мест на экране его сейчас
 *        показывают. Не занимается отрисовкой (круглая маска,
 *        масштабирование) — это ui_icons::realAvatarIcon(), эта же
 *        обёртка только хранит и загружает сырой QImage.
 *
 * imageFor() никогда не блокирует — при промахе кэша она сама запускает
 * (если такой запрос ещё не в процессе и раньше не завершился неудачей)
 * UserProfileClient::fetchAvatar() и возвращает std::nullopt; вызывающая
 * сторона продолжает показывать свою текущую букву-заглушку и
 * подписывается на avatarReady(), чтобы узнать, когда стоит повторно
 * позвать imageFor() и получить настоящую картинку.
 */
class AvatarCache : public QObject {
    Q_OBJECT

public:
    explicit AvatarCache(UserProfileClient& client, QObject* parent = nullptr);

    /// Сырой QImage @p login, если байты уже загружены и декодированы;
    /// иначе std::nullopt (и, если запрос для этого login ещё не в
    /// процессе и раньше не завершился неудачей, запускает его).
    [[nodiscard]] std::optional<QImage> imageFor(const QString& login);

    /// Сбрасывает кэш/неудачу для @p login (issue #442) — вызывается
    /// после собственной успешной загрузки через ProfileDialog: только
    /// что загруженный аватар не должен ждать перезапуска приложения,
    /// чтобы отрисоваться, а прошлая неудача (например, "аватара пока
    /// нет") не должна навсегда блокировать повторные попытки.
    void invalidate(const QString& login);

signals:
    /// @p login только что стал доступен (или изменился) — вызывающая
    /// сторона обычно повторяет imageFor(login) и подставляет результат
    /// в уже показанный виджет.
    void avatarReady(const QString& login);

private:
    UserProfileClient& client_;
    QHash<QString, QImage> images_;
    /// Логины, для которых fetchAvatar() уже в процессе — не даёт
    /// нескольким одновременным вызовам imageFor() для одного и того же
    /// login запустить дублирующиеся сетевые запросы.
    QSet<QString> pending_;
    /// Логины, для которых последняя попытка провалилась (обычно 404 —
    /// у пользователя ещё нет аватара) — не повторяет запрос на каждый
    /// новый imageFor() для того же login, пока invalidate() не снимет
    /// эту отметку.
    QSet<QString> failed_;
};

}  // namespace devicehub
