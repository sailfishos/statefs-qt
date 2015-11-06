#include "tests_common.hpp"
#include <tut/tut.hpp>
#include <tut/tut_console_reporter.hpp>
#include <tut/tut_cppunit_reporter.hpp>
#include <tut/tut_main.hpp>
#include <tut/tut_macros.hpp>
#include <iostream>
#include <QCoreApplication>
#include <QTimer>

void execute_in_event_loop(std::function<void()> fn)
{
    auto app = QCoreApplication::instance();
    auto start = new QTimer(app);
    start->setSingleShot(true);
    start->connect(start, &QTimer::timeout, fn);
    start->setInterval(0);
    start->start();
    app->processEvents();
}

void idle_event_loop()
{
    auto app = QCoreApplication::instance();
    app->processEvents();
}

namespace tut
{
    test_runner_singleton runner;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    tut::console_reporter reporter(std::cerr);
    tut::runner.get().set_callback(&reporter);
    try
    {
        if(tut::tut_main(argc, argv, std::cerr))
        {
            if(reporter.all_ok()) {
                return 0;
            } else {
                std::cerr << std::endl;
                std::cerr << "tests are failed" << std::endl;
            }
        }
    }
    catch(const tut::no_such_group &ex) {
        std::cerr << "No such group: " << ex.what() << std::endl;
    }
    catch(const tut::no_such_test &ex) {
        std::cerr << "No such test: " << ex.what() << std::endl;
    }
    catch(const tut::tut_error &ex) {
        std::cout << "General error: " << ex.what() << std::endl;
    }

    return -1;
}
