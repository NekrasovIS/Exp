#include "ui/StatusLabel.h"

#include <QLabel>
#include <QStyle>

namespace devicehub::ui_status {

namespace {

QString variantName(Variant variant) {
    switch (variant) {
        case Variant::kSuccess:
            return QStringLiteral("success");
        case Variant::kError:
            return QStringLiteral("error");
        case Variant::kNeutral:
            break;
    }
    return QStringLiteral("neutral");
}

}  // namespace

void setStatusText(QLabel* label, const QString& text, Variant variant) {
    label->setText(text);
    label->setProperty("statusVariant", variantName(variant));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

}  // namespace devicehub::ui_status
