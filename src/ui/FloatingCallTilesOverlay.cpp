#include "ui/FloatingCallTilesOverlay.h"

#include <QPushButton>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

namespace {
constexpr int kDefaultWidth = 260;
constexpr int kDefaultHeight = 220;
}  // namespace

FloatingCallTilesOverlay::FloatingCallTilesOverlay(QWidget* parent) : QWidget(parent) {
    // Небольшое отдельное окно (issue #215), тот же приём, что и у
    // CallWindow — родитель нужен только для владения временем жизни
    // через дерево QObject, не для встраивания в layout MainWindow.
    setWindowFlag(Qt::Window, true);
    setWindowTitle(tr("Call (minimized)"));
    resize(kDefaultWidth, kDefaultHeight);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(ui_theme::kSpacingSm, ui_theme::kSpacingSm, ui_theme::kSpacingSm,
                                    ui_theme::kSpacingSm);
    rootLayout->setSpacing(ui_theme::kSpacingSm);

    restoreButton_ = new QPushButton(tr("Expand"), this);
    restoreButton_->setObjectName(QStringLiteral("restoreCallWindowButton"));
    connect(restoreButton_, &QPushButton::clicked, this, &FloatingCallTilesOverlay::restoreRequested);

    // Обычный QWidget без layout'а на момент создания (тот же приём, что
    // и CallWindow::videoStrip_ до первого detachTilesTo()) — CallWindow
    // лениво заводит на нём QGridLayout (issue #437) и сам добавляет
    // туда свои плитки, это окно их геометрией не управляет.
    canvas_ = new QWidget(this);

    rootLayout->addWidget(restoreButton_);
    rootLayout->addWidget(canvas_, /*stretch=*/1);
}

}  // namespace devicehub
