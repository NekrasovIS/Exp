#pragma once

#include <QHash>
#include <QList>
#include <QStringList>
#include <QWidget>

#include "ui/ChatMessageRow.h"

class QHBoxLayout;
class QImage;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTimer;
class QVBoxLayout;
class QVideoWidget;

namespace devicehub {

/**
 * @brief Main content area: shows a placeholder until a channel is
 *        selected in ChannelsPanel, then a grouped, avatar-annotated
 *        message list and send box for that channel.
 *
 * Pure presentation — MainWindow owns ChatClient and feeds messages in
 * via appendMessage()/appendSystemLine()/clearLog(); this class only
 * owns layout, the placeholder/channel toggle, and deciding (via
 * chat_message_grouping) whether a message needs its own header.
 */
class ChatView : public QWidget {
    Q_OBJECT

public:
    explicit ChatView(QWidget* parent = nullptr);

    /// Switches back to the "no channel selected" placeholder.
    void showPlaceholder();

    /// Switches to the chat page and sets its header to @p channelName.
    void showChannel(const QString& channelName);

    /// Marks whether the currently open channel is encrypted (issue
    /// #138) — prefixes the header with a lock, and disables Attach/
    /// Search (not supported for encrypted channels in this phase, so
    /// disabled rather than left to fail server-side). Call after
    /// showChannel() for the channel this applies to.
    void setEncrypted(bool encrypted);

    /// Needed to decide whether an appended message is "own" (bubble
    /// right-aligned, accent-colored, no avatar) or someone else's.
    void setCurrentUserLogin(const QString& login);

    /// Appends a real chat message — grouped under the previous one
    /// (no repeated avatar/name/time) if they're from the same author
    /// within a few minutes of each other.
    void appendMessage(const ChatMessage& message);

    /// Inserts @p messages (chronological, oldest to newest) at the top
    /// of the list, above whatever's already shown — issue #100's
    /// "Load older messages". Grouping between them is computed only
    /// within this batch (compared to whatever was the previously
    /// oldest message shown is deliberately skipped — see class doc
    /// comment). Preserves the user's current scroll position rather
    /// than jumping to the bottom, unlike appendMessage().
    void prependMessages(const QList<ChatMessage>& messages);

    /// Shows/hides the "Load older messages" button above the message
    /// list — MainWindow calls this with whether the last history page
    /// it fetched looked like there might be more (came back full).
    void setLoadOlderVisible(bool visible);

    /// Updates @p id's displayed body in place (issue #107) — a no-op
    /// if that message isn't currently shown (e.g. scrolled out of a
    /// history page that's since been replaced).
    void updateMessageBody(qint64 id, const QString& newBody);

    /// Removes @p id's row entirely, if currently shown.
    void removeMessage(qint64 id);

    /// Scrolls @p id's row into view, if currently shown (issue #118,
    /// jumping to a search result) — @return false if that message isn't
    /// currently loaded (e.g. further back than "Load older" has fetched).
    bool scrollToMessage(qint64 id);

    /// True while the send box is editing an existing message rather
    /// than composing a new one — set by clicking a message's own
    /// "Edit" button, cleared by cancelEditingMessage() or (MainWindow)
    /// once the edit is actually submitted. -1 when not editing.
    [[nodiscard]] qint64 editingMessageId() const { return editingMessageId_; }

    /// Leaves edit mode: clears editingMessageId_, restores the send
    /// button's normal text, and clears the message box.
    void cancelEditingMessage();

    /// Appends a muted, centered system/status line (subscribed,
    /// errors, ...) — always breaks any pending message grouping, so
    /// the next real message gets its own header regardless of author.
    void appendSystemLine(const QString& text);

    /// Clears the message list and resets grouping state.
    void clearLog();

    /// Updates the Call/Leave and Mute/Unmute buttons — MainWindow calls
    /// this after every CallManager state change (join, leave, mute
    /// toggle), rather than this widget tracking call state itself.
    /// Leaving a call (inCall == false) also resets the video strip:
    /// hides it and drops every remote tile, since video can't outlive
    /// the call it belongs to.
    void setCallState(bool inCall, bool muted);

    /// Updates the small "who's in the call" label.
    void setCallParticipants(const QStringList& participants);

    /// Updates the video toggle button and the local preview's
    /// visibility — MainWindow calls this after every
    /// CallManager::enableVideo()/disableVideo().
    void setVideoEnabled(bool enabled);

    /// Same as setVideoEnabled(), for screen share (issue #112) —
    /// MainWindow calls this after every
    /// CallManager::enableScreenShare()/disableScreenShare(). Enabling
    /// one always implies the other is now disabled (CallManager's
    /// video/screen-share are mutually exclusive), so callers should
    /// call both setters together rather than assuming one implies the
    /// other here.
    void setScreenShareEnabled(bool enabled);

    /// Shows (creating its tile on first call) the latest decoded frame
    /// from @p peerLogin's incoming video track.
    void showRemoteVideoFrame(const QString& peerLogin, const QImage& frame);

    /// Drops @p peerLogin's video tile, if it has one.
    void removeRemoteVideo(const QString& peerLogin);

    /// Shows "<login> is typing…" for a few seconds, then auto-hides —
    /// MainWindow calls this from ChatClient::userTyping(). Issue #96:
    /// only the most recently reported typer is shown at a time
    /// (accepted first-version simplification — chat-service has no
    /// "stopped typing" message, so there's no clean way to track a set
    /// of simultaneous typers without one).
    void showTypingUser(const QString& login);

    [[nodiscard]] QWidget* messagesContainer() const { return messagesContainer_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }
    [[nodiscard]] QPushButton* attachButton() const { return attachButton_; }
    [[nodiscard]] QPushButton* callToggleButton() const { return callToggleButton_; }
    [[nodiscard]] QPushButton* muteToggleButton() const { return muteToggleButton_; }
    [[nodiscard]] QPushButton* videoToggleButton() const { return videoToggleButton_; }
    [[nodiscard]] QPushButton* screenShareToggleButton() const { return screenShareToggleButton_; }
    [[nodiscard]] QPushButton* searchButton() const { return searchButton_; }
    [[nodiscard]] QLabel* callParticipantsLabel() const { return callParticipantsLabel_; }
    [[nodiscard]] QVideoWidget* localVideoWidget() const { return localVideoWidget_; }
    [[nodiscard]] QLabel* typingIndicatorLabel() const { return typingIndicatorLabel_; }
    [[nodiscard]] QPushButton* loadOlderButton() const { return loadOlderButton_; }
    /// Кнопка "Show/Hide Chat" (issue #153) — видна только во время
    /// звонка, когда область видео и панель чата делят QSplitter вместо
    /// того, чтобы чат всегда занимал одну и ту же фиксированную долю
    /// окна.
    [[nodiscard]] QPushButton* toggleChatVisibilityButton() const { return toggleChatVisibilityButton_; }

signals:
    /// Emitted when the placeholder's "Create channel" button is
    /// clicked — MainWindow wires this to the same handling as
    /// ChannelsPanel's own "+" button.
    void createChannelRequested();

    /// Call button clicked — MainWindow decides join vs. leave based on
    /// CallManager::inCall() and calls back into setCallState().
    void callToggleRequested();

    /// Mute button clicked — same pattern as callToggleRequested().
    void muteToggleRequested();

    /// Video toggle button clicked — same pattern as
    /// callToggleRequested(): MainWindow decides enable vs. disable
    /// based on CallManager::videoEnabled() and calls back into
    /// setVideoEnabled().
    void videoToggleRequested();

    /// Screen share toggle button clicked — same pattern as
    /// videoToggleRequested().
    void screenShareToggleRequested();

    /// "Search" clicked (issue #118) — MainWindow shows/raises its
    /// SearchDialog.
    void openSearchRequested();

    /// The user is typing in the message box — throttled (at most once
    /// per cooldown window) rather than once per keystroke, so
    /// MainWindow's ChatClient::sendTyping() doesn't spam the network.
    void typingRequested();

    /// "Load older messages" clicked — MainWindow fetches the next page
    /// before the oldest message ChatView currently has.
    void loadOlderMessagesRequested();

    /// "Delete" clicked on one of the user's own messages.
    void deleteMessageRequested(qint64 id);

    /// "Attach" clicked (issue #116) — MainWindow opens a file picker,
    /// uploads the chosen file via ChatRestClient, then auto-sends it as
    /// a message (see MainWindow::onAttachFileClicked()'s doc comment).
    void attachFileRequested();

    /// "Download" clicked on a message with an attachment — bubbled up
    /// from whichever ChatMessageRow it came from.
    void downloadAttachmentRequested(qint64 attachmentId, const QString& filename);

private:
    /// Wires a freshly created row's editRequested()/deleteRequested()
    /// to this view's own edit-mode state / deleteMessageRequested() —
    /// shared by appendMessage() (the only place rows are created,
    /// for now).
    void connectMessageRow(ChatMessageRow* row);

    /// Local preview/strip visibility is "either camera video or screen
    /// share is active" — recomputed from videoActive_/screenShareActive_
    /// rather than each setter's own @p enabled, so calling
    /// setVideoEnabled()/setScreenShareEnabled() in either order after a
    /// mutually-exclusive toggle still leaves the right one visible.
    void updateLocalVideoVisibility();
    /// Re-renders channelTitleLabel_ from currentChannelName_/encrypted_
    /// — shared by showChannel() and setEncrypted() so either can be
    /// called first without one clobbering the other's effect.
    void updateChannelTitleLabel();
    /// Клик по "Hide Chat"/"Show Chat" (issue #153) — сворачивает/
    /// восстанавливает долю chatPanel_ в chatSplitter_, отдавая
    /// остальную часть окна области видео (или, если видео/демонстрация
    /// экрана уже не активны, просто пустому месту).
    void onToggleChatVisibilityClicked();


    QStackedWidget* stack_ = nullptr;
    QLabel* channelTitleLabel_ = nullptr;
    /// The plain channel name, without setEncrypted()'s lock prefix —
    /// showChannel() sets this; setEncrypted() re-derives the label text
    /// from it so the two can be called in either order.
    QString currentChannelName_;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* messagesContainer_ = nullptr;
    QVBoxLayout* messagesLayout_ = nullptr;
    QPushButton* loadOlderButton_ = nullptr;
    QLineEdit* messageEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
    QPushButton* attachButton_ = nullptr;
    QPushButton* callToggleButton_ = nullptr;
    QPushButton* muteToggleButton_ = nullptr;
    QPushButton* videoToggleButton_ = nullptr;
    QPushButton* screenShareToggleButton_ = nullptr;
    bool videoActive_ = false;
    bool screenShareActive_ = false;
    bool encrypted_ = false;
    QPushButton* searchButton_ = nullptr;
    QLabel* callParticipantsLabel_ = nullptr;
    /// Область видео (сверху) и chatPanel_ (снизу) — во время звонка
    /// область видео получает большую часть пространства, а chatPanel_
    /// можно свернуть через toggleChatVisibilityButton_ вместо
    /// фиксированной доли 50/50 (issue #153).
    QSplitter* chatSplitter_ = nullptr;
    /// История (loadOlderButton_/scrollArea_/typingIndicatorLabel_) плюс
    /// строка ввода сообщения — сгруппированы в один виджет, чтобы быть
    /// одним дочерним элементом QSplitter.
    QWidget* chatPanel_ = nullptr;
    QPushButton* toggleChatVisibilityButton_ = nullptr;
    bool chatPanelCollapsed_ = false;
    QWidget* videoStrip_ = nullptr;
    QHBoxLayout* videoStripLayout_ = nullptr;
    QVideoWidget* localVideoWidget_ = nullptr;
    QHash<QString, QLabel*> remoteVideoTiles_;
    QLabel* typingIndicatorLabel_ = nullptr;
    QTimer* typingIndicatorHideTimer_ = nullptr;
    QTimer* typingThrottleTimer_ = nullptr;
    bool hasLastMessage_ = false;
    ChatMessage lastMessage_;
    QString currentUserLogin_;
    /// True while the scrollbar is at (or very near) the bottom — new
    /// messages keep it pinned there, matching a normal chat's
    /// behavior, but stop doing so once the user scrolls up to read
    /// history, and updated as they scroll further. See the
    /// rangeChanged/valueChanged wiring in the constructor.
    bool stickToBottom_ = true;
    qint64 editingMessageId_ = -1;
};

}  // namespace devicehub
