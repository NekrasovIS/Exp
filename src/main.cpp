#include <QApplication>

#include "ui/MainWindow.h"
#include "ui/Theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet(devicehub::ui_theme::darkStyleSheet());

    // MainWindow сам управляет своей видимостью (issue #156) — окно
    // авторизации показывается первым, а сам интерфейс (и его размер
    // 1280x800 из MainWindow::buildUi()) — только после успешного
    // входа/регистрации.
    devicehub::MainWindow window;

    return QApplication::exec();
}
