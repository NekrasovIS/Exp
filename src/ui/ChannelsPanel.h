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
 * @brief Секция боковой панели снизу слева: список каналов текущего
 *        выбранного сообщества, кнопка "+" для создания канала и меню
 *        по правому клику для переименования/удаления.
 *
 * Чистое представление, тот же паттерн, что и CommunitiesPanel —
 * MainWindow отслеживает, какое сообщество выбрано, и соответствующим
 * образом наполняет содержимое этой панели; клик по каналу здесь
 * открывает его в ChatView.
 */
class ChannelsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ChannelsPanel(QWidget* parent = nullptr);

    /// Заменяет содержимое списка (каналы того сообщества, которое
    /// сейчас выбрано — пусто, если сообщество не выбрано).
    void setChannels(const QList<ChatItem>& channels);

    /// Выбирает элемент с @p id, если он есть, не порождая при этом
    /// channelSelected() — см. CommunitiesPanel::selectCommunityId().
    void selectChannelId(qint64 id);

    /// Нужно, чтобы решить, предлагать ли переименование/удаление
    /// для элемента.
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
