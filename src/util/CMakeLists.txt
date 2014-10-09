set(CMAKE_AUTOMOC TRUE)

if(ENABLE_QT4)

include_directories(
  ${QT_QTCORE_INCLUDE_DIRS}
)
link_directories(
  ${QT_QTCORE_LIBRARY_DIRS}
)

add_library(statefs-qt4
  SHARED
  util.cpp
)
target_link_libraries(statefs-qt4
  ${QT_CORE_LIBRARIES}
  ${COR_LIBRARIES}
  ${QTAROUND_LIBRARIES}
#  ${STATEFS_LIBRARIES}
)

configure_file(statefs-qt4.pc.in statefs-qt4.pc @ONLY)
  
install(TARGETS statefs-qt4 DESTINATION ${DST_LIB})
install(FILES statefs-qt4.pc DESTINATION ${DST_LIB}/pkgconfig)

else(ENABLE_QT4)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
)
link_directories(
  ${Qt5Core_LIBRARY_DIRS}
)

add_library(statefs-qt5
  SHARED
  util.cpp
)
target_link_libraries(statefs-qt5
  ${Qt5Core_LIBRARIES}
  ${COR_LIBRARIES}
  ${QTAROUND_LIBRARIES}
#  ${STATEFS_LIBRARIES}
)

configure_file(statefs-qt5.pc.in statefs-qt5.pc @ONLY)
  
install(TARGETS statefs-qt5 DESTINATION ${DST_LIB})
install(FILES statefs-qt5.pc DESTINATION ${DST_LIB}/pkgconfig)

endif(ENABLE_QT4)
