set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC TRUE)
find_package(Qt5Core REQUIRED)

configure_install_pkgconfig(contextkit-statefs)
configure_install_pkgconfig(contextsubscriber-1.0)

qt5_wrap_cpp(LIB_MOC_SRC
  ${STATEFS_QT_MOC_HEADERS}
  actor.hpp
)

add_library(contextkit-statefs-qt5
  SHARED
  property.cpp
  ${LIB_MOC_SRC}
)
target_link_libraries(contextkit-statefs-qt5
  ${Qt5Core_LIBRARIES}
  statefs-qt5
  ${COR_LIBRARIES}
  ${QTAROUND_LIBRARIES}
)

install(TARGETS contextkit-statefs-qt5 DESTINATION ${DST_LIB})
