project(statefs-qt)
cmake_minimum_required(VERSION 2.8)

IF(NOT DEFINED VERSION)
  message(fatal_error "define VERSION")
ENDIF(NOT DEFINED VERSION)

find_package(PkgConfig REQUIRED)
find_package(Cor REQUIRED)
pkg_check_modules(QTAROUND qtaround REQUIRED)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  #  ${STATEFS_INCLUDES}
  ${COR_INCLUDE_DIRS}
  ${QTAROUND_INCLUDE_DIRS}
)

link_directories(
  #  ${STATEFS_LIBS}
  ${COR_LIBRARY_DIRS}
  ${QTAROUND_LIBRARY_DIRS}
)

add_custom_target(doc
  COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile)

find_package(Qt5Core REQUIRED)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
)

link_directories(
  ${Qt5Core_LIBRARY_DIRS}
)

install(DIRECTORY include/statefs DESTINATION include/qt5)
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/contextsubscriber
#  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
install(FILES include/contextsubscriber/contextproperty.h DESTINATION include)
set(STATEFS_QT_MOC_HEADERS
  ${CMAKE_SOURCE_DIR}/include/contextsubscriber/contextproperty.h
  ${CMAKE_SOURCE_DIR}/include/statefs/qt/client.hpp
)

add_subdirectory(src/util)
add_subdirectory(src/contextkit-subscriber)
add_subdirectory(src/qml)
add_subdirectory(tests)
add_subdirectory(tools/monitor)

MESSAGE(STATUS "VERSION=${VERSION}")
