#include "auth/AuthClient.h"
#include "chat/ChatRestClient.h"

#include <gtest/gtest.h>

#include <QDateTime>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstdlib>

// Requires a live auth-service + user-service (to mint a real token)
// and chat-service (REST). Skips itself rather than failing when the
// stack isn't running — same pattern as ChatClientIntegrationTest.cpp.

namespace devicehub {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

QUrl authServiceUrl() {
    return QUrl(QString::fromStdString(envOrDefault("AUTH_SERVICE_URL", "http://127.0.0.1:8080")));
}

QUrl chatRestUrl() {
    return QUrl(QString::fromStdString(envOrDefault("CHAT_SERVICE_URL", "http://127.0.0.1:8082")));
}

QString uniqueLogin(const QString& prefix) {
    return prefix + QStringLiteral("-") + QString::number(QDateTime::currentMSecsSinceEpoch());
}

// Registers a brand-new user against a live auth-service and returns
// its auto-issued token, or an empty string if the stack isn't
// reachable within the timeout.
QString registerAndGetToken(const QString& loginPrefix) {
    AuthClient authClient(authServiceUrl());
    QString token;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&authClient, &AuthClient::tokenReceived, &loop, [&](const QString& receivedToken) {
        token = receivedToken;
        loop.quit();
    });
    QObject::connect(&authClient, &AuthClient::errorOccurred, &loop, [&](const QString&) { loop.quit(); });
    authClient.registerUser(uniqueLogin(loginPrefix), QStringLiteral("chat-rest-client-test-password"));
    loop.exec();
    return token;
}

TEST(ChatRestClientTest, CreateCommunityEmitsCommunityCreated) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-create-community"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 createdId = 0;
    QString createdName;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString& name) {
        createdId = id;
        createdName = name;
        loop.quit();
    });
    client.createCommunity(token, QStringLiteral("chat-rest-test-community"));
    loop.exec();

    EXPECT_GT(createdId, 0);
    EXPECT_EQ(createdName, QStringLiteral("chat-rest-test-community"));
}

TEST(ChatRestClientTest, ListCommunitiesIncludesCreatedCommunity) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-list-communities"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 createdId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            createdId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-listed-community"));
        loop.exec();
    }
    ASSERT_GT(createdId, 0);

    QList<ChatItem> listed;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::communitiesListed, &loop, [&](const QList<ChatItem>& communities) {
        listed = communities;
        loop.quit();
    });
    client.listCommunities(token);
    loop.exec();

    const bool found = std::any_of(listed.begin(), listed.end(), [&](const ChatItem& item) { return item.id == createdId; });
    EXPECT_TRUE(found);
}

TEST(ChatRestClientTest, RenameCommunityEmitsCommunityRenamed) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-rename-community"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-to-be-renamed"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    qint64 renamedId = 0;
    QString renamedTo;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::communityRenamed, &loop, [&](qint64 id, const QString& newName) {
        renamedId = id;
        renamedTo = newName;
        loop.quit();
    });
    client.renameCommunity(token, communityId, QStringLiteral("chat-rest-renamed"));
    loop.exec();

    EXPECT_EQ(renamedId, communityId);
    EXPECT_EQ(renamedTo, QStringLiteral("chat-rest-renamed"));
}

TEST(ChatRestClientTest, RenameCommunityByNonOwnerEmitsErrorWithServerDetail) {
    const QString ownerToken = registerAndGetToken(QStringLiteral("chat-rest-rename-owner"));
    const QString intruderToken = registerAndGetToken(QStringLiteral("chat-rest-rename-intruder"));
    if (ownerToken.isEmpty() || intruderToken.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient ownerClient(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&ownerClient, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        ownerClient.createCommunity(ownerToken, QStringLiteral("chat-rest-protected"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    ChatRestClient intruderClient(chatRestUrl());
    QString errorMessage;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&intruderClient, &ChatRestClient::errorOccurred, &loop, [&](const QString& message) {
        errorMessage = message;
        loop.quit();
    });
    intruderClient.renameCommunity(intruderToken, communityId, QStringLiteral("hijacked"));
    loop.exec();

    // The server's own detail ("only the owner can do that"), not a
    // generic "server replied: Forbidden" — this is exactly the bug
    // extractErrorMessage() was written to fix (see issue #41).
    EXPECT_FALSE(errorMessage.isEmpty());
    EXPECT_NE(errorMessage.indexOf(QStringLiteral("owner")), -1);
}

TEST(ChatRestClientTest, DeleteCommunityEmitsCommunityDeleted) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-delete-community"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-to-be-deleted"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    qint64 deletedId = 0;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::communityDeleted, &loop, [&](qint64 id) {
        deletedId = id;
        loop.quit();
    });
    client.deleteCommunity(token, communityId);
    loop.exec();

    EXPECT_EQ(deletedId, communityId);
}

TEST(ChatRestClientTest, JoinCommunityEmitsCommunityJoined) {
    const QString ownerToken = registerAndGetToken(QStringLiteral("chat-rest-join-owner"));
    const QString joinerToken = registerAndGetToken(QStringLiteral("chat-rest-join-joiner"));
    if (ownerToken.isEmpty() || joinerToken.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient ownerClient(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&ownerClient, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        ownerClient.createCommunity(ownerToken, QStringLiteral("chat-rest-joinable"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    ChatRestClient joinerClient(chatRestUrl());
    qint64 joinedId = 0;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&joinerClient, &ChatRestClient::communityJoined, &loop, [&](qint64 id) {
        joinedId = id;
        loop.quit();
    });
    joinerClient.joinCommunity(joinerToken, communityId);
    loop.exec();

    EXPECT_EQ(joinedId, communityId);
}

TEST(ChatRestClientTest, CreateChannelEmitsChannelCreated) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-create-channel"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-channel-parent"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    qint64 createdChannelId = 0;
    QString createdChannelName;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::channelCreated, &loop, [&](qint64 id, const QString& name) {
        createdChannelId = id;
        createdChannelName = name;
        loop.quit();
    });
    client.createChannel(token, communityId, QStringLiteral("general"));
    loop.exec();

    EXPECT_GT(createdChannelId, 0);
    EXPECT_EQ(createdChannelName, QStringLiteral("general"));
}

TEST(ChatRestClientTest, ListChannelsIncludesCreatedChannel) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-list-channels"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-list-channels-parent"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    qint64 channelId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::channelCreated, &loop, [&](qint64 id, const QString&) {
            channelId = id;
            loop.quit();
        });
        client.createChannel(token, communityId, QStringLiteral("general"));
        loop.exec();
    }
    ASSERT_GT(channelId, 0);

    QList<ChatItem> listed;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::channelsListed, &loop, [&](const QList<ChatItem>& channels) {
        listed = channels;
        loop.quit();
    });
    client.listChannels(token, communityId);
    loop.exec();

    const bool found = std::any_of(listed.begin(), listed.end(), [&](const ChatItem& item) { return item.id == channelId; });
    EXPECT_TRUE(found);
}

TEST(ChatRestClientTest, RenameChannelEmitsChannelRenamed) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-rename-channel"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-rename-channel-parent"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    qint64 channelId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::channelCreated, &loop, [&](qint64 id, const QString&) {
            channelId = id;
            loop.quit();
        });
        client.createChannel(token, communityId, QStringLiteral("general"));
        loop.exec();
    }
    ASSERT_GT(channelId, 0);

    qint64 renamedId = 0;
    QString renamedTo;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::channelRenamed, &loop, [&](qint64 id, const QString& newName) {
        renamedId = id;
        renamedTo = newName;
        loop.quit();
    });
    client.renameChannel(token, channelId, QStringLiteral("renamed-channel"));
    loop.exec();

    EXPECT_EQ(renamedId, channelId);
    EXPECT_EQ(renamedTo, QStringLiteral("renamed-channel"));
}

TEST(ChatRestClientTest, DeleteChannelEmitsChannelDeleted) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-delete-channel"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-delete-channel-parent"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    qint64 channelId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::channelCreated, &loop, [&](qint64 id, const QString&) {
            channelId = id;
            loop.quit();
        });
        client.createChannel(token, communityId, QStringLiteral("general"));
        loop.exec();
    }
    ASSERT_GT(channelId, 0);

    qint64 deletedId = 0;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::channelDeleted, &loop, [&](qint64 id) {
        deletedId = id;
        loop.quit();
    });
    client.deleteChannel(token, channelId);
    loop.exec();

    EXPECT_EQ(deletedId, channelId);
}

TEST(ChatRestClientTest, CreateChannelInNonexistentCommunityEmitsError) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-channel-404"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    QString errorMessage;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ChatRestClient::errorOccurred, &loop, [&](const QString& message) {
        errorMessage = message;
        loop.quit();
    });
    client.createChannel(token, 999999999, QStringLiteral("nowhere"));
    loop.exec();

    EXPECT_FALSE(errorMessage.isEmpty());
}

TEST(ChatRestClientTest, PromoteThenListThenDemoteModeratorRoundTrip) {
    const QString token = registerAndGetToken(QStringLiteral("chat-rest-mod-owner"));
    if (token.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient client(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        client.createCommunity(token, QStringLiteral("chat-rest-mod-community"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    // Doesn't need to be a real registered account — chat-service's
    // memberships table has no foreign key into user-service's users.
    const QString targetLogin = uniqueLogin(QStringLiteral("chat-rest-mod-target"));

    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        qint64 promotedCommunityId = 0;
        QString promotedLogin;
        QObject::connect(&client, &ChatRestClient::moderatorPromoted, &loop,
                          [&](qint64 id, const QString& login) {
                              promotedCommunityId = id;
                              promotedLogin = login;
                              loop.quit();
                          });
        client.promoteModerator(token, communityId, targetLogin);
        loop.exec();
        EXPECT_EQ(promotedCommunityId, communityId);
        EXPECT_EQ(promotedLogin, targetLogin);
    }

    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QStringList listedLogins;
        QObject::connect(&client, &ChatRestClient::moderatorsListed, &loop,
                          [&](qint64, const QStringList& logins) {
                              listedLogins = logins;
                              loop.quit();
                          });
        client.listModerators(token, communityId);
        loop.exec();
        EXPECT_EQ(listedLogins, QStringList{targetLogin});
    }

    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QString demotedLogin;
        QObject::connect(&client, &ChatRestClient::moderatorDemoted, &loop,
                          [&](qint64, const QString& login) {
                              demotedLogin = login;
                              loop.quit();
                          });
        client.demoteModerator(token, communityId, targetLogin);
        loop.exec();
        EXPECT_EQ(demotedLogin, targetLogin);
    }

    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QStringList listedAfterDemote;
        bool receivedList = false;
        QObject::connect(&client, &ChatRestClient::moderatorsListed, &loop,
                          [&](qint64, const QStringList& logins) {
                              listedAfterDemote = logins;
                              receivedList = true;
                              loop.quit();
                          });
        client.listModerators(token, communityId);
        loop.exec();
        ASSERT_TRUE(receivedList);
        EXPECT_TRUE(listedAfterDemote.isEmpty());
    }
}

TEST(ChatRestClientTest, PromoteModeratorByNonOwnerEmitsError) {
    const QString ownerToken = registerAndGetToken(QStringLiteral("chat-rest-mod-403-owner"));
    const QString intruderToken = registerAndGetToken(QStringLiteral("chat-rest-mod-403-intruder"));
    if (ownerToken.isEmpty() || intruderToken.isEmpty()) {
        GTEST_SKIP() << "auth-service/user-service not reachable — start the stack to run this test.";
    }

    ChatRestClient ownerClient(chatRestUrl());
    qint64 communityId = 0;
    {
        QEventLoop loop;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        QObject::connect(&ownerClient, &ChatRestClient::communityCreated, &loop, [&](qint64 id, const QString&) {
            communityId = id;
            loop.quit();
        });
        ownerClient.createCommunity(ownerToken, QStringLiteral("chat-rest-mod-403-community"));
        loop.exec();
    }
    ASSERT_GT(communityId, 0);

    ChatRestClient intruderClient(chatRestUrl());
    QString errorMessage;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QObject::connect(&intruderClient, &ChatRestClient::errorOccurred, &loop, [&](const QString& message) {
        errorMessage = message;
        loop.quit();
    });
    intruderClient.promoteModerator(intruderToken, communityId, uniqueLogin(QStringLiteral("anyone")));
    loop.exec();

    EXPECT_FALSE(errorMessage.isEmpty());
}

}  // namespace
}  // namespace devicehub
