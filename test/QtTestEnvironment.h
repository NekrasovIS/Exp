#pragma once

#include <QApplication>
#include <gtest/gtest.h>

#include <memory>

namespace devicehub_test {

/**
 * @brief Shared GTest environment providing a live QApplication.
 *
 * Qt Widgets (MainWindow), Qt Multimedia device enumeration, and
 * QGuiApplication::screens() all need a live application instance to
 * initialize their backends — GTest's own main() alone isn't enough.
 * Shared across test files rather than duplicated per file.
 */
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override;

private:
    std::unique_ptr<QApplication> app_;
};

}  // namespace devicehub_test
