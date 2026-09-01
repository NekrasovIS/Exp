#pragma once

#include <QWidget>

#include "chat/ChatRestClient.h"

class QLineEdit;
class QListWidget;
class QPoint;
class QPushButton;
class QStackedWidget;

namespace devicehub {

/**
 * @brief Bottom-left sidebar section: the channel list of whichever
 *        community is currently selected, a "+" button to create one,
 *        and a right-click menu to rename/delete.
 *
 * Pure presentation, same pattern as CommunitiesPanel — MainWindow
 * tracks which community is selected and supplies this panel's
 * contents accordingly; clicking a channel here is what opens it in
 * ChatView.
 */
class ChannelsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ChannelsPanel(QWidget* parent = nullptr);

    /// Replaces the list contents (the channels of whichever community
    /// is currently selected — empty when no community is selected).
    void setChannels(const QList<ChatItem>& channels);

    /// Selects the item with @p id, if present, without emitting
    /// channelSelected() — see CommunitiesPanel::selectCommunityId().
    void selectChannelId(qint64 id);

    /// Needed to decide whether to offer rename/delete for an item.
    void setCurrentUserLogin(const QString& login);

    [[nodiscard]] QListWidget* listWidget() const { return listWidget_; }
    [[nodiscard]] QPushButton* addButton() const { return addButton_; }
    [[nodiscard]] QPushButton* refreshButton() const { return refreshButton_; }
    /// Поле фильтра над списком (issue #152) — ввод текста сужает
    /// listWidget() до строк, чьё имя его содержит.
    [[nodiscard]] QLineEdit* filterEdit() const { return filterEdit_; }

signals:
    void createRequested(const QString& name, bool isEncrypted);
    void renameRequested(qint64 id, const QString& newName);
    void deleteRequested(qint64 id);
    void channelSelected(qint64 id, const QString& name);

private:
    void showAddDialog();
    void showContextMenu(const QPoint& pos);
    /// Скрывает строки, чьё сохранённое имя не содержит текущий текст
    /// filterEdit_ (без учёта регистра) — вызывается при каждом
    /// изменении filterEdit_ и после того, как setChannels()
    /// пересоздаёт список.
    void applyFilter();

    QListWidget* listWidget_ = nullptr;
    QStackedWidget* listStack_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QLineEdit* filterEdit_ = nullptr;
    QString currentUserLogin_;
};

}  // namespace devicehub
