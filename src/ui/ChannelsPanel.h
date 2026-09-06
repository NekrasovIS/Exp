#pragma once

#include <QDateTime>
#include <QHash>
#include <QWidget>

#include "chat/ChatRestClient.h"

class QLineEdit;
class QListWidget;
class QListWidgetItem;
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

    /// Обновляет кэш «последнее сообщение» для канала @p channelId
    /// (issue #152) — превью показывается второй строкой под именем в
    /// списке, @p activityAt поднимает канал в сортировке по последней
    /// активности. @p previewText уже готов к показу как есть (для
    /// зашифрованных каналов вызывающий код передаёт плейсхолдер вместо
    /// шифротекста — эта панель ничего не знает про шифрование).
    /// @p messageId — id этого сообщения; индикатор непрочитанного
    /// зажигается сам, если он больше, чем id последнего сообщения,
    /// прочитанного в этом канале (см. setOpenChannelId()) — простое
    /// клиентское состояние без нового серверного API, как и просит
    /// issue #152.
    void recordChannelActivity(qint64 channelId, qint64 messageId, const QString& previewText,
                                const QDateTime& activityAt);

    /// Отмечает @p channelId как открытый прямо сейчас — снимает его
    /// индикатор непрочитанного и запоминает id последнего известного
    /// сообщения как "прочитано", так что более поздние
    /// recordChannelActivity() для этого канала не зажигают индикатор,
    /// пока он остаётся открытым (issue #152). Передать -1, если сейчас
    /// не открыт ни один канал.
    void setOpenChannelId(qint64 channelId);

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
    /// Последнее известное сообщение и его активность для одного
    /// канала — см. recordChannelActivity()/setOpenChannelId().
    struct ChannelActivity {
        QString previewText;
        QDateTime activityAt;
        qint64 lastMessageId = -1;
        bool unread = false;
    };

    void showAddDialog();
    void showContextMenu(const QPoint& pos);
    /// Скрывает строки, чьё сохранённое имя не содержит текущий текст
    /// filterEdit_ (без учёта регистра) — вызывается при каждом
    /// изменении filterEdit_ и после того, как rebuildList()
    /// пересоздаёт список.
    void applyFilter();
    /// Пересобирает listWidget_ из channels_, отсортированных по
    /// activity_ (каналы с известной активностью — по убыванию времени,
    /// первыми; остальные — в порядке, как их вернул сервер), и
    /// накладывает превью/индикатор непрочитанного на каждую строку.
    void rebuildList();

    QListWidget* listWidget_ = nullptr;
    QStackedWidget* listStack_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QLineEdit* filterEdit_ = nullptr;
    QString currentUserLogin_;
    QList<ChatItem> channels_;
    QHash<qint64, ChannelActivity> activity_;
    QHash<qint64, qint64> lastReadMessageId_;
    qint64 openChannelId_ = -1;
};

}  // namespace devicehub
