#include "ui/LoginWindow.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(LoginWindowTest, ClickingRequestCodeWithEmptyIdentifierShowsStatusAndEmitsNothing) {
    LoginWindow window;
    QSignalSpy spy(&window, &LoginWindow::requestCodeRequested);

    window.requestCodeButton()->click();

    EXPECT_EQ(spy.count(), 0);
    EXPECT_FALSE(window.statusLabel()->text().isEmpty());
}

TEST(LoginWindowTest, ClickingRequestCodeWithIdentifierEmitsRequestCodeRequested) {
    LoginWindow window;
    window.identifierEdit()->setText(QStringLiteral("alice@example.test"));
    QSignalSpy spy(&window, &LoginWindow::requestCodeRequested);

    window.requestCodeButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("alice@example.test"));
}

TEST(LoginWindowTest, PressingEnterInIdentifierEditEmitsRequestCodeRequested) {
    LoginWindow window;
    window.identifierEdit()->setText(QStringLiteral("alice"));
    QSignalSpy spy(&window, &LoginWindow::requestCodeRequested);

    emit window.identifierEdit()->returnPressed();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("alice"));
}

TEST(LoginWindowTest, ShowCodeSentSwitchesToTheCodeStepAndClearsStatus) {
    LoginWindow window;
    window.identifierEdit()->setText(QStringLiteral("alice"));
    window.requestCodeButton()->click();

    // QStackedWidget explicitly hides/shows the page widget itself, not
    // each individual child on it — isHidden() on a leaf widget like
    // codeEdit() only reflects whether *that exact widget* was ever
    // told to hide, not its ancestors (that's isVisible(), which needs
    // the whole chain up to a shown top level — see isHidden()'s own
    // docs). Checking the immediate parent (the page QStackedWidget
    // actually manages) gives the right answer without needing a real
    // show() cycle in this headless test.
    EXPECT_TRUE(window.codeEdit()->parentWidget()->isHidden());

    window.showCodeSent(QStringLiteral("alice@example.test"));

    EXPECT_FALSE(window.codeEdit()->parentWidget()->isHidden());
    EXPECT_TRUE(window.identifierEdit()->parentWidget()->isHidden());
}

TEST(LoginWindowTest, ClickingVerifyCodeWithEmptyCodeShowsStatusAndEmitsNothing) {
    LoginWindow window;
    window.showCodeSent(QStringLiteral("alice@example.test"));
    QSignalSpy spy(&window, &LoginWindow::verifyCodeRequested);

    window.verifyCodeButton()->click();

    EXPECT_EQ(spy.count(), 0);
    EXPECT_FALSE(window.statusLabel()->text().isEmpty());
}

TEST(LoginWindowTest, ClickingVerifyCodeEmitsVerifyCodeRequestedWithTheOriginalIdentifier) {
    LoginWindow window;
    window.identifierEdit()->setText(QStringLiteral("alice@example.test"));
    window.requestCodeButton()->click();
    window.showCodeSent(QStringLiteral("alice@example.test"));
    window.codeEdit()->setText(QStringLiteral("123456"));
    QSignalSpy spy(&window, &LoginWindow::verifyCodeRequested);

    window.verifyCodeButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("alice@example.test"));
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("123456"));
}

TEST(LoginWindowTest, PressingEnterInCodeEditEmitsVerifyCodeRequested) {
    LoginWindow window;
    window.showCodeSent(QStringLiteral("alice"));
    window.codeEdit()->setText(QStringLiteral("654321"));
    QSignalSpy spy(&window, &LoginWindow::verifyCodeRequested);

    emit window.codeEdit()->returnPressed();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("654321"));
}

TEST(LoginWindowTest, BackButtonReturnsToIdentifierStepAndClearsCode) {
    LoginWindow window;
    window.showCodeSent(QStringLiteral("alice"));
    window.codeEdit()->setText(QStringLiteral("111111"));

    window.backButton()->click();

    EXPECT_TRUE(window.codeEdit()->parentWidget()->isHidden());
    EXPECT_FALSE(window.identifierEdit()->parentWidget()->isHidden());
    EXPECT_TRUE(window.codeEdit()->text().isEmpty());
}

TEST(LoginWindowTest, ShowErrorSetsStatusLabelText) {
    LoginWindow window;

    window.showError(QStringLiteral("invalid or expired code"));

    EXPECT_TRUE(window.statusLabel()->text().contains(QStringLiteral("invalid or expired code")));
}

TEST(LoginWindowTest, ResetReturnsToIdentifierStepAndClearsEverything) {
    LoginWindow window;
    window.identifierEdit()->setText(QStringLiteral("alice"));
    window.showCodeSent(QStringLiteral("alice"));
    window.codeEdit()->setText(QStringLiteral("111111"));
    window.showError(QStringLiteral("some error"));

    window.reset();

    EXPECT_TRUE(window.codeEdit()->parentWidget()->isHidden());
    EXPECT_TRUE(window.identifierEdit()->text().isEmpty());
    EXPECT_TRUE(window.codeEdit()->text().isEmpty());
    EXPECT_TRUE(window.statusLabel()->text().isEmpty());

    // reset() 's identifier clearing round-trips: requesting a code
    // again afterwards must not resurrect the old pending identifier.
    window.identifierEdit()->setText(QStringLiteral("bob"));
    QSignalSpy spy(&window, &LoginWindow::requestCodeRequested);
    window.requestCodeButton()->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("bob"));
}

}  // namespace
}  // namespace devicehub
