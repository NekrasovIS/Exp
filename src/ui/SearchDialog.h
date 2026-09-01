#pragma once

#include <QDialog>
#include <QList>

#include "chat/ChatRestClient.h"

class QLineEdit;
class QListWidget;
class QPushButton;

namespace devicehub {

/**
 * @brief Поиск сообщений: поле запроса и список результатов для
 *        текущего открытого канала — открывается кнопкой "Search" у
 *        ChatView.
 *
 * Чистое представление — MainWindow владеет ChatRestClient и передаёт
 * результаты через setResults(), тот же паттерн, что и у SettingsDialog,
 * которая сама не владеет объектами устройств.
 */
class SearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SearchDialog(QWidget* parent = nullptr);

    [[nodiscard]] QLineEdit* queryEdit() const { return queryEdit_; }
    [[nodiscard]] QPushButton* searchButton() const { return searchButton_; }
    [[nodiscard]] QListWidget* resultsList() const { return resultsList_; }

    /// Заменяет список результатов на @p matches (сначала новые, как
    /// возвращает ChatRestClient::messagesFound()).
    void setResults(const QList<ChatMessageInfo>& matches);

    /// Очищает список результатов и поле запроса — вызывается при
    /// переключении канала, чтобы не показывались устаревшие результаты
    /// из другого канала.
    void clearResults();

signals:
    /// Клик по "Search" или нажатие Enter в поле запроса.
    void searchRequested(const QString& query);

    /// Строка результата была активирована (двойной клик/Enter) —
    /// @p messageId — сообщение, к которому нужно перейти, если оно
    /// сейчас загружено в ChatView.
    void resultActivated(qint64 messageId);

private:
    QLineEdit* queryEdit_ = nullptr;
    QPushButton* searchButton_ = nullptr;
    QListWidget* resultsList_ = nullptr;
};

}  // namespace devicehub
