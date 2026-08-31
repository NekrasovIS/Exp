#pragma once

#include <QDialog>
#include <QList>

#include "chat/ChatRestClient.h"

class QLineEdit;
class QListWidget;
class QPushButton;

namespace devicehub {

/**
 * @brief Message search: a query box and a results list for the
 *        currently open channel — opened from ChatView's "Search"
 *        button.
 *
 * Pure presentation — MainWindow owns ChatRestClient and feeds results
 * in via setResults(), same pattern as SettingsDialog owning no device
 * objects itself.
 */
class SearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SearchDialog(QWidget* parent = nullptr);

    [[nodiscard]] QLineEdit* queryEdit() const { return queryEdit_; }
    [[nodiscard]] QPushButton* searchButton() const { return searchButton_; }
    [[nodiscard]] QListWidget* resultsList() const { return resultsList_; }

    /// Replaces the results list with @p matches (newest first, as
    /// returned by ChatRestClient::messagesFound()).
    void setResults(const QList<ChatMessageInfo>& matches);

    /// Clears the results list and query box — called on channel switch
    /// so stale results from a different channel aren't shown.
    void clearResults();

signals:
    /// "Search" clicked, or Enter pressed in the query box.
    void searchRequested(const QString& query);

    /// A result row was activated (double-click/Enter) — @p messageId
    /// is the message to jump to, if it's currently loaded in ChatView.
    void resultActivated(qint64 messageId);

private:
    QLineEdit* queryEdit_ = nullptr;
    QPushButton* searchButton_ = nullptr;
    QListWidget* resultsList_ = nullptr;
};

}  // namespace devicehub
