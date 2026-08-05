#include "ui/ToastBanner.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QTimer>

#include "ui/Theme.h"

namespace devicehub {

namespace {

constexpr int kTopInset = 12;
constexpr int kSideInset = 12;

QString variantName(ToastBanner::Variant variant) {
    switch (variant) {
        case ToastBanner::Variant::kSuccess:
            return QStringLiteral("success");
        case ToastBanner::Variant::kError:
            return QStringLiteral("error");
        case ToastBanner::Variant::kInfo:
            return QStringLiteral("info");
    }
    return QStringLiteral("info");
}

}  // namespace

ToastBanner::ToastBanner(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("toastBanner"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(ui_theme::kSpacingMd, ui_theme::kSpacingSm, ui_theme::kSpacingMd,
                                ui_theme::kSpacingSm);

    label_ = new QLabel(this);
    label_->setWordWrap(true);
    layout->addWidget(label_);

    hideTimer_ = new QTimer(this);
    hideTimer_->setSingleShot(true);
    connect(hideTimer_, &QTimer::timeout, this, &QWidget::hide);

    parent->installEventFilter(this);
    hide();
}

void ToastBanner::showMessage(const QString& text, Variant variant, int timeoutMs) {
    label_->setText(text);
    setProperty("variant", variantName(variant));
    style()->unpolish(this);
    style()->polish(this);

    reposition();
    show();
    raise();
    hideTimer_->start(timeoutMs);
}

bool ToastBanner::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        reposition();
    }
    return QWidget::eventFilter(watched, event);
}

void ToastBanner::reposition() {
    const int width = parentWidget()->width() - 2 * kSideInset;
    setGeometry(kSideInset, kTopInset, width, sizeHint().height());
}

}  // namespace devicehub
