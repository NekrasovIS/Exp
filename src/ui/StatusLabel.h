#pragma once

class QLabel;
class QString;

namespace devicehub::ui_status {

/// Смысловой вариант статусного сообщения — определяет цвет текста
/// через Theme.cpp (см. QLabel[statusVariant="..."]), а не то, что
/// сам вызывающий код рисует вручную.
enum class Variant {
    kNeutral,  ///< обычный ход дела (прогресс, подсказка) — цвет по умолчанию.
    kSuccess,  ///< действие завершилось успехом.
    kError,    ///< действие не выполнено/заблокировано.
};

/// Задаёт текст @p label и вариант его цвета одновременно — статусные
/// QLabel'ы (ProfileDialog/ModeratorsDialog/LoginWindow/SettingsDialog)
/// живут дольше одного сообщения, поэтому смена варианта на уже
/// показанном виджете требует repolish (то же самое, что уже делает
/// ToastBanner::showMessage() при смене своего "variant").
void setStatusText(QLabel* label, const QString& text, Variant variant = Variant::kNeutral);

}  // namespace devicehub::ui_status
