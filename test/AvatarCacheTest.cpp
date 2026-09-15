#include "user/AvatarCache.h"

#include "user/UserProfileClient.h"

#include <gtest/gtest.h>

#include <QBuffer>
#include <QByteArray>
#include <QEventLoop>
#include <QImage>
#include <QSignalSpy>
#include <QTimer>
#include <QUrl>

// Не требует запущенного user-service: UserProfileClient сконструирован
// на заведомо недоступный порт (127.0.0.1:1 — привилегированный, ничего
// не слушает), поэтому реальный сетевой запрос всегда завершается
// avatarFetchFailed() быстро и детерминированно — этого достаточно,
// чтобы проверить дедупликацию/повторные попытки AvatarCache без живого
// backend'а. Успешный путь (imageFor() после удачной загрузки) вызывает
// UserProfileClient::avatarFetched() напрямую как обычную функцию —
// сигналы Qt всегда public-функции (см. qobjectdefs.h), это тот же
// приём, что и обычный `emit`, просто без реального сетевого round trip.

namespace devicehub {
namespace {

QUrl unreachableBaseUrl() {
    return QUrl(QStringLiteral("http://127.0.0.1:1"));
}

QByteArray encodeTinyPng(Qt::GlobalColor color) {
    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

/// Крутит цикл событий, пока @p signal не сработает хотя бы раз, либо
/// не истечёт @p timeoutMs — используется вместо QSignalSpy::wait()
/// только там, где нужен и таймаут, и совместимость с обычным лямбда-
/// коннектом.
template <typename Sender, typename Signal>
void spinUntil(Sender* sender, Signal signal, int timeoutMs) {
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    QObject::connect(sender, signal, &loop, &QEventLoop::quit);
    loop.exec();
}

TEST(AvatarCacheTest, ImageForOnCacheMissReturnsNulloptAndTriggersExactlyOneFetchForRepeatedCalls) {
    UserProfileClient client(unreachableBaseUrl());
    AvatarCache cache(client);
    QSignalSpy failedSpy(&client, &UserProfileClient::avatarFetchFailed);

    // Два быстрых вызова для одного и того же login, пока первый запрос
    // ещё не завершился — второй не должен запустить дублирующийся
    // fetchAvatar().
    EXPECT_FALSE(cache.imageFor(QStringLiteral("alice")).has_value());
    EXPECT_FALSE(cache.imageFor(QStringLiteral("alice")).has_value());

    spinUntil(&client, &UserProfileClient::avatarFetchFailed, 5000);

    EXPECT_EQ(failedSpy.count(), 1);
}

TEST(AvatarCacheTest, ImageForDoesNotRetryAfterFailureUntilInvalidated) {
    UserProfileClient client(unreachableBaseUrl());
    AvatarCache cache(client);
    QSignalSpy failedSpy(&client, &UserProfileClient::avatarFetchFailed);

    static_cast<void>(cache.imageFor(QStringLiteral("bob")));
    spinUntil(&client, &UserProfileClient::avatarFetchFailed, 5000);
    ASSERT_EQ(failedSpy.count(), 1);

    // Повторный вызов после уже провалившейся попытки не должен запустить
    // новый сетевой запрос, пока invalidate() не снимет отметку "неудача".
    EXPECT_FALSE(cache.imageFor(QStringLiteral("bob")).has_value());
    QEventLoop settleLoop;
    QTimer::singleShot(300, &settleLoop, &QEventLoop::quit);
    settleLoop.exec();
    EXPECT_EQ(failedSpy.count(), 1);

    cache.invalidate(QStringLiteral("bob"));
    EXPECT_FALSE(cache.imageFor(QStringLiteral("bob")).has_value());
    spinUntil(&client, &UserProfileClient::avatarFetchFailed, 5000);
    EXPECT_EQ(failedSpy.count(), 2);
}

TEST(AvatarCacheTest, ImageForReturnsDecodedImageAfterASuccessfulFetchAndEmitsAvatarReady) {
    UserProfileClient client(unreachableBaseUrl());
    AvatarCache cache(client);
    QSignalSpy readySpy(&cache, &AvatarCache::avatarReady);

    EXPECT_FALSE(cache.imageFor(QStringLiteral("carol")).has_value());

    client.avatarFetched(QStringLiteral("carol"), encodeTinyPng(Qt::red), QStringLiteral("image/png"));

    ASSERT_EQ(readySpy.count(), 1);
    EXPECT_EQ(readySpy.at(0).at(0).toString(), QStringLiteral("carol"));

    const std::optional<QImage> cached = cache.imageFor(QStringLiteral("carol"));
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->size(), QSize(4, 4));
}

TEST(AvatarCacheTest, AvatarFetchedWithUndecodableBytesMarksTheLoginAsFailedRatherThanCachingGarbage) {
    UserProfileClient client(unreachableBaseUrl());
    AvatarCache cache(client);
    QSignalSpy readySpy(&cache, &AvatarCache::avatarReady);
    QSignalSpy failedSpy(&client, &UserProfileClient::avatarFetchFailed);

    static_cast<void>(cache.imageFor(QStringLiteral("dan")));
    client.avatarFetched(QStringLiteral("dan"), QByteArrayLiteral("not a real image"), QStringLiteral("image/png"));

    EXPECT_EQ(readySpy.count(), 0);
    // Ведёт себя как обычная неудача — imageFor() не пытается снова,
    // пока не позвать invalidate() (то же поведение, что и после
    // avatarFetchFailed() — см. предыдущий тест).
    EXPECT_FALSE(cache.imageFor(QStringLiteral("dan")).has_value());
    QEventLoop settleLoop;
    QTimer::singleShot(300, &settleLoop, &QEventLoop::quit);
    settleLoop.exec();
    EXPECT_EQ(failedSpy.count(), 0);
}

TEST(AvatarCacheTest, InvalidateClearsACachedImageForcingAFreshFetchOnNextCall) {
    UserProfileClient client(unreachableBaseUrl());
    AvatarCache cache(client);

    static_cast<void>(cache.imageFor(QStringLiteral("erin")));
    client.avatarFetched(QStringLiteral("erin"), encodeTinyPng(Qt::blue), QStringLiteral("image/png"));
    ASSERT_TRUE(cache.imageFor(QStringLiteral("erin")).has_value());

    cache.invalidate(QStringLiteral("erin"));

    EXPECT_FALSE(cache.imageFor(QStringLiteral("erin")).has_value());
}

}  // namespace
}  // namespace devicehub
