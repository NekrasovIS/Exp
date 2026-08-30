#pragma once

class QString;

namespace devicehub::notification_policy {

/// True if a new chat message from @p messageAuthor should trigger a
/// desktop notification — not while the main window is already active
/// (the user is looking at it), and never for the user's own messages
/// (chat-service echoes every message back to its sender too, see
/// ChatClient's doc comment). @p currentUserLogin empty (not signed in
/// yet) never suppresses a notification on the author-match check.
bool shouldNotify(bool windowActive, const QString& messageAuthor, const QString& currentUserLogin);

}  // namespace devicehub::notification_policy
