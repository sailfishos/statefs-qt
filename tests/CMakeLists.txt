if(ENABLE_QT4)

include_directories(
  ${QT_INCLUDE_DIR}
  ${CONTEXTSUBSCRIBER_INCLUDE_DIRS}
)
set(SUBSCRIBER_LIB contextkit-statefs-qt4)

else(ENABLE_QT4)

set(SUBSCRIBER_LIB contextkit-statefs-qt5)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

add_definitions(
  ${Qt5Core_DEFINITIONS}
)

endif(ENABLE_QT4)


add_executable(atest-statefs_contextkit_subscriber_linking
  statefs_contextkit_subscriber_linking.cpp)


if(ENABLE_QT4)
target_link_libraries(atest-statefs_contextkit_subscriber_linking
  ${QT_QTCORE_LIBRARY}
  ${SUBSCRIBER_LIB}
)
else(ENABLE_QT4)
target_link_libraries(atest-statefs_contextkit_subscriber_linking
  ${Qt5Core_LIBRARIES}
  ${SUBSCRIBER_LIB}
)
endif(ENABLE_QT4)
