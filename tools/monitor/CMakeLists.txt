if(ENABLE_QT4)
# target_link_libraries(contextkit-monitor
#   ${QT_QTCORE_LIBRARY}
# )

MESSAGE(STATUS "Monitor tool is not built for Qt4")

else(ENABLE_QT4)

# fix for gcc 4.6 specific compiling/linking issues, no dwarf-4
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -gdwarf-3"
  )

set(CMAKE_AUTOMOC TRUE)

add_executable(contextkit-monitor monitor.cpp)
target_link_libraries(contextkit-monitor
  contextkit-statefs-qt5
  statefs-qt5
)

target_link_libraries(contextkit-monitor
  ${Qt5Core_LIBRARIES}
)
install(TARGETS contextkit-monitor DESTINATION bin)

add_executable(file-monitor filemon.cpp)

target_link_libraries(file-monitor
  ${Qt5Core_LIBRARIES}
)

endif(ENABLE_QT4)
