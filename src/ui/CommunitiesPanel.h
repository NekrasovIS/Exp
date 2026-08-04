#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;

namespace devicehub {

/**
 * @brief Top-left sidebar section: create/list/join communities.
 *
 * Pure presentation — exposes its controls so MainWindow can wire them to
 * ChatRestClient the same way it always has; this class only owns layout.
 */
class CommunitiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit CommunitiesPanel(QWidget* parent = nullptr);

    [[nodiscard]] QLineEdit* nameEdit() const { return nameEdit_; }
    [[nodiscard]] QPushButton* createButton() const { return createButton_; }
    [[nodiscard]] QComboBox* communityCombo() const { return communityCombo_; }
    [[nodiscard]] QPushButton* refreshButton() const { return refreshButton_; }
    [[nodiscard]] QPushButton* joinButton() const { return joinButton_; }

private:
    QLineEdit* nameEdit_ = nullptr;
    QPushButton* createButton_ = nullptr;
    QComboBox* communityCombo_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QPushButton* joinButton_ = nullptr;
};

}  // namespace devicehub
