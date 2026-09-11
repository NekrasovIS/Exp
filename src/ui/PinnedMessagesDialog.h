#pragma once

#include <QDialog>
#include <QList>

#include "chat/ChatRestClient.h"

class QListWidget;

namespace devicehub {

/**
 * @brief Список закреплённых сообщений открытого канала (issue #338) —
 *        открывается кнопкой "📌 N" в шапке ChatView.
 *
 * Чистое представление — MainWindow владеет ChatRestClient/ChatClient и
 * передаёт список через setPinnedMessages(), тот же паттерн, что и у
 * SearchDialog (список результатов поиска), которую этот класс
 * сознательно копирует по форме — обе показывают список сообщений,
 * клик по элементу которого переходит к нему в ChatView.
 */
class PinnedMessagesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PinnedMessagesDialog(QWidget* parent = nullptr);

    [[nodiscard]] QListWidget* pinnedList() const { return pinnedList_; }

    /// Заменяет список на @p pinned (в том порядке, в каком отдаёт
    /// сервер — самые новые закрепления первыми).
    void setPinnedMessages(const QList<PinnedMessageInfo>& pinned);

signals:
    /// Строка была активирована (двойной клик/Enter) — @p messageId —
    /// сообщение, к которому нужно перейти, если оно сейчас загружено
    /// в ChatView.
    void messageActivated(qint64 messageId);

private:
    QListWidget* pinnedList_ = nullptr;
};

}  // namespace devicehub
