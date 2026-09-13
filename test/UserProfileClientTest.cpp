#include "auth/AuthClient.h"
#include "user/UserProfileClient.h"

#include <gtest/gtest.h>

#include <QDateTime>
#include <QEventLoop>
#include <QMessageAuthenticationCode>
#include <QTimer>
#include <QUrl>

#include <cstdlib>
#include <utility>

// Требует запущенных auth-service + user-service (маршруты профиля из
// issue #110 находятся в user-service). Пропускает себя вместо падения,
// если стек не запущен — тот же паттерн, что и в ChatRestClientTest.cpp.

namespace devicehub {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

QUrl authServiceUrl() {
    return QUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
}

QUrl userServiceUrl() {
    return QUrl(QString::fromStdString(envOrDefault("USER_SERVICE_URL", "http://127.0.0.1:8081")));
}

QString uniqueLogin(const QString& prefix) {
    return prefix + QStringLiteral("-") + QString::number(QDateTime::currentMSecsSinceEpoch());
}

// Регистрирует нового пользователя на живом auth-service и возвращает
// {token, login}, либо две пустые строки, если стек недоступен в течение
// тайм-аута.
std::pair<QString, QString> registerAndGetTokenAndLogin(const QString& loginPrefix) {
    AuthClient authClient(authServiceUrl());
    const QString login = uniqueLogin(loginPrefix);
    QString token;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&authClient, &AuthClient::tokenReceived, &loop, [&](const QString& receivedToken) {
        token = receivedToken;
        loop.quit();
    });
    QObject::connect(&authClient, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
    authClient.registerUser(login, QStringLiteral("user-profile-client-test-password"));
    loop.exec();
    return {token, login};
}

TEST(UserProfileClientTest, UpdateOwnProfileRoundTripsThroughFetchProfile) {
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-update"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    // Unique per run, same as login — a hardcoded email literal would
    // collide with a previous run's row on user-service's partial
    // unique index (issue #156) the moment this binary runs a second
    // time against the same database, turning updateOwnProfile() into
    // an unrelated 409 that this test (only listening for
    // profileUpdated(), not errorOccurred()) would just silently time
    // out on instead of failing with a clear message.
    const QString email = login + QStringLiteral("@example.test");

    UserProfileClient client(userServiceUrl());
    UserProfile updated;
    QString updateError;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileUpdated, &loop, [&](const UserProfile& profile) {
            updated = profile;
            loop.quit();
        });
        QObject::connect(&client, &UserProfileClient::errorOccurred, &loop, [&](const QString& message) {
            updateError = message;
            loop.quit();
        });
        client.updateOwnProfile(token,
                                 ProfileEdits{.displayName = QStringLiteral("Alice"),
                                              .avatarUrl = QStringLiteral("https://example.test/alice.png"),
                                              .email = email,
                                              .telegramChatId = login + QStringLiteral("-chat")});
        loop.exec();
    }
    ASSERT_TRUE(updateError.isEmpty()) << updateError.toStdString();
    EXPECT_EQ(updated.login, login);
    EXPECT_EQ(updated.displayName, QStringLiteral("Alice"));
    EXPECT_EQ(updated.avatarUrl, QStringLiteral("https://example.test/alice.png"));
    EXPECT_EQ(updated.email, email);
    EXPECT_EQ(updated.telegramChatId, login + QStringLiteral("-chat"));

    UserProfile fetched;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileReceived, &loop, [&](const UserProfile& profile) {
            fetched = profile;
            loop.quit();
        });
        client.fetchProfile(token, login);
        loop.exec();
    }
    EXPECT_EQ(fetched.displayName, QStringLiteral("Alice"));
    EXPECT_EQ(fetched.avatarUrl, QStringLiteral("https://example.test/alice.png"));
    EXPECT_EQ(fetched.email, email);
    EXPECT_EQ(fetched.telegramChatId, login + QStringLiteral("-chat"));
}

TEST(UserProfileClientTest, PublishPublicKeyRoundTripsThroughFetchProfile) {
    // issue #136 (сквозное шифрование, фаза 1): публикация ключа — отдельный
    // вызов от updateOwnProfile(); этот тест подтверждает, что он попадает в
    // тот же самый профиль, не затрагивая display_name/avatar_url (на
    // сервере это fetch, затем слияние — fetch-then-merge).
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-pubkey"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    UserProfileClient client(userServiceUrl());
    UserProfile updated;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileUpdated, &loop, [&](const UserProfile& profile) {
            updated = profile;
            loop.quit();
        });
        client.publishPublicKey(token, QStringLiteral("base64-x25519-public-key"));
        loop.exec();
    }
    EXPECT_EQ(updated.login, login);
    EXPECT_EQ(updated.publicKey, QStringLiteral("base64-x25519-public-key"));

    UserProfile fetched;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::profileReceived, &loop, [&](const UserProfile& profile) {
            fetched = profile;
            loop.quit();
        });
        client.fetchProfile(token, login);
        loop.exec();
    }
    EXPECT_EQ(fetched.publicKey, QStringLiteral("base64-x25519-public-key"));
}

// Issue #388/#390 — вычисляет TOTP-код так же, как это делало бы
// приложение-аутентификатор, чтобы тест полного флоу настройки TOTP мог
// подтвердить setupTotp() и позвать disableTotp() без реального
// аутентификатора. Своя маленькая копия RFC 4226/6238 — DeviceHub
// зависит от libsodium, которая намеренно не предоставляет SHA-1
// (устаревший примитив), поэтому HMAC-SHA1 берётся из Qt
// (QMessageAuthenticationCode) вместо ещё одной vcpkg-зависимости.
QByteArray decodeBase32ForTest(const QString& text) {
    static const QString kAlphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
    QString bits;
    for (const QChar rawChar : text) {
        if (rawChar == QLatin1Char('=')) {
            break;
        }
        const int index = kAlphabet.indexOf(rawChar.toUpper());
        if (index < 0) {
            continue;
        }
        bits += QString::number(index, 2).rightJustified(5, QLatin1Char('0'));
    }
    QByteArray bytes;
    for (int i = 0; i + 8 <= bits.size(); i += 8) {
        bytes.append(static_cast<char>(bits.mid(i, 8).toUShort(nullptr, 2)));
    }
    return bytes;
}

QString computeTotpCodeForTest(const QString& base32Secret) {
    const QByteArray key = decodeBase32ForTest(base32Secret);
    const uint64_t counter = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch() / 30);

    QByteArray counterBytes(8, '\0');
    for (int i = 7; i >= 0; --i) {
        counterBytes[i] = static_cast<char>((counter >> ((7 - i) * 8)) & 0xff);
    }

    QMessageAuthenticationCode hmacCode(QCryptographicHash::Sha1);
    hmacCode.setKey(key);
    hmacCode.addData(counterBytes);
    const QByteArray digest = hmacCode.result();

    const auto offset = static_cast<unsigned char>(digest[digest.size() - 1] & 0x0f);
    const uint32_t truncated = ((static_cast<unsigned char>(digest[offset]) & 0x7f) << 24) |
                                ((static_cast<unsigned char>(digest[offset + 1]) & 0xff) << 16) |
                                ((static_cast<unsigned char>(digest[offset + 2]) & 0xff) << 8) |
                                (static_cast<unsigned char>(digest[offset + 3]) & 0xff);
    return QString::number(truncated % 1000000).rightJustified(6, QLatin1Char('0'));
}

TEST(UserProfileClientTest, TotpSetupConfirmStatusAndDisableRoundTrip) {
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-totp"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    UserProfileClient client(userServiceUrl());

    bool initialStatus = true;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::totpStatusReceived, &loop, [&](bool enabled) {
            initialStatus = enabled;
            loop.quit();
        });
        client.fetchTotpStatus(token);
        loop.exec();
    }
    ASSERT_FALSE(initialStatus) << "a freshly registered account must not have TOTP enabled";

    TotpSetupInfo setup;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::totpSetupStarted, &loop, [&](const TotpSetupInfo& info) {
            setup = info;
            loop.quit();
        });
        client.setupTotp(token);
        loop.exec();
    }
    ASSERT_FALSE(setup.secret.isEmpty());
    ASSERT_FALSE(setup.otpauthUrl.isEmpty());

    QStringList backupCodes;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::totpConfirmed, &loop, [&](const QStringList& codes) {
            backupCodes = codes;
            loop.quit();
        });
        client.confirmTotp(token, computeTotpCodeForTest(setup.secret));
        loop.exec();
    }
    EXPECT_EQ(backupCodes.size(), 10);

    bool statusAfterConfirm = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::totpStatusReceived, &loop, [&](bool enabled) {
            statusAfterConfirm = enabled;
            loop.quit();
        });
        client.fetchTotpStatus(token);
        loop.exec();
    }
    EXPECT_TRUE(statusAfterConfirm);

    bool disabled = false;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::totpDisabled, &loop, [&]() {
            disabled = true;
            loop.quit();
        });
        client.disableTotp(token, computeTotpCodeForTest(setup.secret));
        loop.exec();
    }
    EXPECT_TRUE(disabled);
}

TEST(UserProfileClientTest, ConfirmTotpWithAWrongCodeEmitsError) {
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-totp-bad-code"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    UserProfileClient client(userServiceUrl());
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &UserProfileClient::totpSetupStarted, &loop, [&](const TotpSetupInfo&) {
            loop.quit();
        });
        client.setupTotp(token);
        loop.exec();
    }

    QString errorMessage;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &UserProfileClient::errorOccurred, &loop, [&](const QString& message) {
        errorMessage = message;
        loop.quit();
    });
    client.confirmTotp(token, QStringLiteral("000000"));
    loop.exec();

    EXPECT_FALSE(errorMessage.isEmpty());
}

TEST(UserProfileClientTest, FetchProfileWithoutAuthorizationEmitsError) {
    const auto [token, login] = registerAndGetTokenAndLogin(QStringLiteral("user-profile-client-noauth"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    UserProfileClient client(userServiceUrl());
    QString errorMessage;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &UserProfileClient::errorOccurred, &loop, [&](const QString& message) {
        errorMessage = message;
        loop.quit();
    });
    client.fetchProfile(QStringLiteral("not-a-real-token"), login);
    loop.exec();

    EXPECT_FALSE(errorMessage.isEmpty());
}

}  // namespace
}  // namespace devicehub
