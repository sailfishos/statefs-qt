set(CMAKE_AUTOMOC TRUE)

configure_file(contextkit-statefs.pc.in contextkit-statefs.pc @ONLY)
configure_file(contextsubscriber-1.0.pc.in contextsubscriber-1.0.pc @ONLY)

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
install(FILES contextkit-statefs.pc DESTINATION ${DST_LIB}/pkgconfig)
install(FILES contextsubscriber-1.0.pc DESTINATION ${DST_LIB}/pkgconfig)
