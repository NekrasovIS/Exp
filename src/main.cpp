#include <QApplication>

#include "ui/MainWindow.h"
#include "ui/Theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet(devicehub::ui_theme::discordDarkStyleSheet());

    devicehub::MainWindow window;
    window.resize(480, 800);
    window.show();

    return QApplication::exec();
}
