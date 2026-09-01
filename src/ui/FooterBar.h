#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace devicehub {

/**
 * @brief Нижняя панель: мини-профиль текущего пользователя слева и
 *        кнопка-шестерёнка, открывающая SettingsDialog.
 */
class FooterBar : public QWidget {
    Q_OBJECT

public:
    explicit FooterBar(QWidget* parent = nullptr);

    /// Обновляет текст профиля (например, логин вошедшего пользователя
    /// или заглушку "не выполнен вход").
    void setProfileText(const QString& text);

    [[nodiscard]] QPushButton* settingsButton() const { return settingsButton_; }

private:
    QLabel* avatarLabel_ = nullptr;
    QLabel* profileLabel_ = nullptr;
    QPushButton* settingsButton_ = nullptr;
};

}  // namespace devicehub
