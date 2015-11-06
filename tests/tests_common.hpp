#ifndef _STATEFS_QT_TESTS_COMMON_HPP_
#define _STATEFS_QT_TESTS_COMMON_HPP_

#include <functional>

void execute_in_event_loop(std::function<void()>);
void idle_event_loop();

#endif // _STATEFS_QT_TESTS_COMMON_HPP_
