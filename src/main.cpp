#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    devicehub::MainWindow window;
    window.resize(480, 800);
    window.show();

    return QApplication::exec();
}
