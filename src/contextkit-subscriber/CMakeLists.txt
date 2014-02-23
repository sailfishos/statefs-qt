set(CMAKE_AUTOMOC TRUE)

if(ENABLE_QT4)

configure_file(contextkit-statefs-qt4.pc.in contextkit-statefs-qt4.pc @ONLY)

# dirty hack: absolute path, moc is $$$
qt4_wrap_cpp(LIB_MOC_SRC
  /usr/include/contextsubscriber/contextproperty.h property.hpp actor.hpp
)

add_library(contextkit-statefs-qt4
  SHARED property.cpp
  ${LIB_MOC_SRC}
)
target_link_libraries(contextkit-statefs-qt4
  ${QT_QTCORE_LIBRARY}
  statefs-qt4
  ${COR_LIBRARIES}
)

install(TARGETS contextkit-statefs-qt4 DESTINATION ${DST_LIB})
install(FILES contextkit-statefs-qt4.pc DESTINATION ${DST_LIB}/pkgconfig)

else(ENABLE_QT4)

configure_file(contextkit-statefs.pc.in contextkit-statefs.pc @ONLY)

qt5_wrap_cpp(LIB_MOC_SRC
  ${CMAKE_SOURCE_DIR}/include/contextsubscriber/contextproperty.h actor.hpp
)

add_library(contextkit-statefs-qt5
  SHARED property.cpp
  ${LIB_MOC_SRC}
)
target_link_libraries(contextkit-statefs-qt5
  ${Qt5Core_LIBRARIES}
  statefs-qt5
  ${COR_LIBRARIES}
)

install(TARGETS contextkit-statefs-qt5 DESTINATION ${DST_LIB})
install(FILES contextkit-statefs.pc DESTINATION ${DST_LIB}/pkgconfig)

endif(ENABLE_QT4)
