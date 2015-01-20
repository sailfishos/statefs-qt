set(CMAKE_AUTOMOC TRUE)

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

configure_install_pkgconfig(statefs-qt5)
  
install(TARGETS statefs-qt5 DESTINATION ${DST_LIB})
