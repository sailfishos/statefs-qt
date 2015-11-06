#include "tests_common.hpp"
#include <tut/tut.hpp>
#include <contextproperty.h>
#include <QDebug>
#include <QCoreApplication>
#include <functional>

namespace tut
{

struct subscriber_test
{
    virtual ~subscriber_test()
    {
    }
};

typedef test_group<subscriber_test> tf;
typedef tf::object object;
tf vault_subscriber_test("subscriber");

enum test_ids {
    tid_race_condition =  1
};

static QString property1Name("Unknown.NonExistent");
static QString property2Name("Battery.ChargePercentage");
static QString &propertyName = property1Name;

template<> template<>
void object::test<tid_race_condition>()
{
    auto test_fn = [](std::function<void()> idle) {
        for (int i = 0; i < 1000; ++i) {
            delete new ContextProperty(propertyName);
        }
        for (int i = 0; i < 1000; ++i) {
            delete new ContextProperty(propertyName);
            idle();
        }
        for (int i = 0; i < 1000; ++i) {
            auto p = new ContextProperty(propertyName);
            idle();
            p->subscribe();
            idle();
            p->unsubscribe();
            idle();
            p->subscribe();
            idle();
            delete p;
            idle();
            delete new ContextProperty(propertyName);
            idle();
        }
    };
    test_fn([]() {});
    auto test_events_fn = std::bind(test_fn, idle_event_loop);
    execute_in_event_loop(test_events_fn);
    propertyName = property2Name;
    test_fn([]() {});
    execute_in_event_loop(test_events_fn);
}

}
