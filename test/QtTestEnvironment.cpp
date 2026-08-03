#include "QtTestEnvironment.h"

namespace devicehub_test {

void QtTestEnvironment::SetUp() {
    static int argc = 1;
    static char argv0[] = "devicehub_tests";
    static char* argv[] = {argv0};
    if (QCoreApplication::instance() == nullptr) {
        app_ = std::make_unique<QApplication>(argc, argv);
    }
}

namespace {
const ::testing::Environment* const kQtEnv = ::testing::AddGlobalTestEnvironment(new QtTestEnvironment());
}  // namespace

}  // namespace devicehub_test
