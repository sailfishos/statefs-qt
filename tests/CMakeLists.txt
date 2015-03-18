pkg_check_modules(TUT REQUIRED tut>=0.0.3)
include_directories(${TUT_INCLUDES})

testrunner_project(statefs-qt5)
set(UNIT_TESTS subscriber)

set(SUBSCRIBER_LIB contextkit-statefs-qt5)

MACRO(UNIT_TEST _name)
  set(_exe_name test_${_name})
  add_executable(${_exe_name} main.cpp ${_name}.cpp)
  target_link_libraries(${_exe_name}
    ${SUBSCRIBER_LIB}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    )
  qt5_use_modules(${_exe_name} Core)
  testrunner_install(TARGETS ${_exe_name})
ENDMACRO(UNIT_TEST)

foreach(t ${UNIT_TESTS})
  UNIT_TEST(${t})
endforeach(t)

add_executable(atest-statefs_contextkit_subscriber_linking
  statefs_contextkit_subscriber_linking.cpp)

qt5_use_modules(atest-statefs_contextkit_subscriber_linking Core)

target_link_libraries(atest-statefs_contextkit_subscriber_linking
  ${SUBSCRIBER_LIB}
)
