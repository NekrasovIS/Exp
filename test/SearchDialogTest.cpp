#include "ui/SearchDialog.h"

#include <gtest/gtest.h>

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(SearchDialogTest, ClickingSearchButtonEmitsSearchRequestedWithQueryText) {
    SearchDialog dialog;
    dialog.queryEdit()->setText(QStringLiteral("hello"));

    QSignalSpy spy(&dialog, &SearchDialog::searchRequested);
    dialog.searchButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("hello"));
}

TEST(SearchDialogTest, PressingEnterInQueryEditEmitsSearchRequested) {
    SearchDialog dialog;
    dialog.queryEdit()->setText(QStringLiteral("world"));

    QSignalSpy spy(&dialog, &SearchDialog::searchRequested);
    emit dialog.queryEdit()->returnPressed();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("world"));
}

TEST(SearchDialogTest, SetResultsPopulatesListWithAuthorBodyAndTime) {
    SearchDialog dialog;

    dialog.setResults({ChatMessageInfo{.id = 7,
                                        .author = QStringLiteral("alice"),
                                        .body = QStringLiteral("hi there"),
                                        .sentAt = QStringLiteral("2026-08-05 09:00:00")}});

    ASSERT_EQ(dialog.resultsList()->count(), 1);
    const QString text = dialog.resultsList()->item(0)->text();
    EXPECT_TRUE(text.contains(QStringLiteral("alice")));
    EXPECT_TRUE(text.contains(QStringLiteral("hi there")));
    EXPECT_EQ(dialog.resultsList()->item(0)->data(Qt::UserRole).toLongLong(), 7);
}

TEST(SearchDialogTest, SetResultsReplacesPreviousContents) {
    SearchDialog dialog;
    dialog.setResults(
        {ChatMessageInfo{.id = 1, .author = "a", .body = "one", .sentAt = "t"}});

    dialog.setResults(
        {ChatMessageInfo{.id = 2, .author = "b", .body = "two", .sentAt = "t"},
         ChatMessageInfo{.id = 3, .author = "c", .body = "three", .sentAt = "t"}});

    EXPECT_EQ(dialog.resultsList()->count(), 2);
}

TEST(SearchDialogTest, ClearResultsEmptiesListAndQueryBox) {
    SearchDialog dialog;
    dialog.queryEdit()->setText(QStringLiteral("stale query"));
    dialog.setResults({ChatMessageInfo{.id = 1, .author = "a", .body = "one", .sentAt = "t"}});

    dialog.clearResults();

    EXPECT_EQ(dialog.resultsList()->count(), 0);
    EXPECT_TRUE(dialog.queryEdit()->text().isEmpty());
}

TEST(SearchDialogTest, ActivatingResultItemEmitsResultActivatedWithMessageId) {
    SearchDialog dialog;
    dialog.setResults({ChatMessageInfo{.id = 42, .author = "a", .body = "found me", .sentAt = "t"}});

    QSignalSpy spy(&dialog, &SearchDialog::resultActivated);
    emit dialog.resultsList()->itemActivated(dialog.resultsList()->item(0));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toLongLong(), 42);
}

}  // namespace
}  // namespace devicehub
