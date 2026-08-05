#pragma once

#include <QWidget>

class QLabel;
class QTimer;

namespace devicehub {

/**
 * @brief Transient, non-modal feedback banner floating over its parent
 *        (top-inset, full width) — used for CRUD success/error/info
 *        messages that a QMainWindow::statusBar() line is too easy to
 *        miss.
 *
 * Positions itself via an event filter on the parent (repositions on
 * resize) rather than participating in the parent's layout, since it
 * needs to float above whatever content is already there.
 */
class ToastBanner : public QWidget {
    Q_OBJECT

public:
    enum class Variant { kSuccess, kError, kInfo };

    /// @p parent is the widget the banner floats over (e.g. ChatView) —
    /// must not be null.
    explicit ToastBanner(QWidget* parent);

    /// Shows @p text for @p timeoutMs milliseconds, styled per @p variant.
    /// Calling again while already visible restarts the timeout with the
    /// new text/variant.
    void showMessage(const QString& text, Variant variant, int timeoutMs);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void reposition();

    QLabel* label_ = nullptr;
    QTimer* hideTimer_ = nullptr;
};

}  // namespace devicehub
