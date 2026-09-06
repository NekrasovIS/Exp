#pragma once

#include <QApplication>
#include <gtest/gtest.h>

#include <memory>

namespace devicehub_test {

/**
 * @brief Общее для всех тестов GTest-окружение, предоставляющее живой QApplication.
 *
 * Qt Widgets (MainWindow), перечисление устройств Qt Multimedia и
 * QGuiApplication::screens() — всем им нужен живой экземпляр приложения для
 * инициализации своих бэкендов — одного main() из GTest недостаточно.
 * Используется совместно всеми тестовыми файлами, а не дублируется в каждом.
 */
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override;

private:
    std::unique_ptr<QApplication> app_;
};

}  // namespace devicehub_test
