set(SUBSCRIBER_LIB contextkit-statefs-qt5)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

add_definitions(
  ${Qt5Core_DEFINITIONS}
)

add_executable(atest-statefs_contextkit_subscriber_linking
  statefs_contextkit_subscriber_linking.cpp)

target_link_libraries(atest-statefs_contextkit_subscriber_linking
  ${Qt5Core_LIBRARIES}
  ${SUBSCRIBER_LIB}
)
