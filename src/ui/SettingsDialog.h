#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QVideoWidget;

namespace devicehub {

/**
 * @brief Настройки устройств: вывод аудио, микрофон, камера, захват
 *        экрана — по одной вкладке на каждое, открывается кнопкой-
 *        шестерёнкой у FooterBar.
 *
 * Чистое представление (те же элементы управления, что были на старых
 * вкладках верхнего уровня, просто перенесённые в диалог) — MainWindow
 * по-прежнему владеет объектами устройств и всей связующей логикой.
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    [[nodiscard]] QComboBox* outputCombo() const { return outputCombo_; }
    [[nodiscard]] QPushButton* playToneButton() const { return playToneButton_; }

    [[nodiscard]] QComboBox* inputCombo() const { return inputCombo_; }
    [[nodiscard]] QPushButton* toggleMicButton() const { return toggleMicButton_; }
    [[nodiscard]] QProgressBar* micLevelBar() const { return micLevelBar_; }
    [[nodiscard]] QLabel* micStatusLabel() const { return micStatusLabel_; }

    [[nodiscard]] QComboBox* cameraCombo() const { return cameraCombo_; }
    [[nodiscard]] QPushButton* toggleCameraButton() const { return toggleCameraButton_; }
    [[nodiscard]] QVideoWidget* videoPreview() const { return videoPreview_; }
    [[nodiscard]] QLabel* cameraStatusLabel() const { return cameraStatusLabel_; }

    [[nodiscard]] QComboBox* screenCombo() const { return screenCombo_; }
    [[nodiscard]] QPushButton* toggleScreenCaptureButton() const { return toggleScreenCaptureButton_; }
    [[nodiscard]] QVideoWidget* screenPreview() const { return screenPreview_; }
    [[nodiscard]] QLabel* screenStatusLabel() const { return screenStatusLabel_; }

private:
    QComboBox* outputCombo_ = nullptr;
    QPushButton* playToneButton_ = nullptr;

    QComboBox* inputCombo_ = nullptr;
    QPushButton* toggleMicButton_ = nullptr;
    QProgressBar* micLevelBar_ = nullptr;
    QLabel* micStatusLabel_ = nullptr;

    QComboBox* cameraCombo_ = nullptr;
    QPushButton* toggleCameraButton_ = nullptr;
    QVideoWidget* videoPreview_ = nullptr;
    QLabel* cameraStatusLabel_ = nullptr;

    QComboBox* screenCombo_ = nullptr;
    QPushButton* toggleScreenCaptureButton_ = nullptr;
    QVideoWidget* screenPreview_ = nullptr;
    QLabel* screenStatusLabel_ = nullptr;
};

}  // namespace devicehub
