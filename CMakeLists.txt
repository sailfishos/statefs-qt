project(statefs-qt)
cmake_minimum_required(VERSION 2.8)

IF(NOT DEFINED VERSION)
  message(fatal_error "define VERSION")
ENDIF(NOT DEFINED VERSION)

option(ENABLE_QT4 "Build Qt4 version of libraries" OFF)

option(ENABLE_MULTIARCH "enable support for multi-arch distros (lib64)" OFF)
IF(ENABLE_MULTIARCH)
  IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
    SET(LIB_SUFFIX "")
  ELSE(CMAKE_SIZEOF_VOID_P EQUAL 4)
    SET(LIB_SUFFIX 64)
  ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 4)
ELSE()
  SET(LIB_SUFFIX "")
ENDIF()
SET(DST_LIB lib${LIB_SUFFIX})

find_package(PkgConfig REQUIRED)
pkg_check_modules(COR cor REQUIRED)
pkg_check_modules(QTAROUND qtaround REQUIRED)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wall -O2 -g -feliminate-unused-debug-types -std=c++0x -fPIC"
  )

#-Wno-psabi is to remove next g++ warning/note:
#the mangling of 'va_list' has changed in GCC 4.4
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wno-psabi"
)

#pkg_check_modules(STATEFS statefs-cpp REQUIRED)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  #  ${STATEFS_INCLUDES}
  ${COR_INCLUDES}
  ${QTAROUND_INCLUDE_DIRS}
)

link_directories(
  #  ${STATEFS_LIBS}
  ${COR_LIBRARY_DIRS}
)

add_custom_target(doc
  COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile)


if(ENABLE_QT4)

find_package(Qt4 4.7.0 REQUIRED)
include(${QT_USE_FILE})
add_definitions(-DQT_SHARED)

include_directories(
  ${QT_INCLUDE_DIR}
)
link_directories(
  ${QT_LIBRARY_DIRS}
)

install(DIRECTORY include/statefs DESTINATION include/qt4)

pkg_check_modules(CONTEXTSUBSCRIBER contextsubscriber-1.0 REQUIRED)
pkg_check_modules(CONTEXTPROVIDER contextprovider-1.0 REQUIRED)
include_directories(
  ${CONTEXTSUBSCRIBER_INCLUDE_DIRS}
  ${CONTEXTPROVIDER_INCLUDE_DIRS}
)

else(ENABLE_QT4)

find_package(Qt5Core REQUIRED)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
)

link_directories(
  ${Qt5Core_LIBRARY_DIRS}
)

install(DIRECTORY include/statefs DESTINATION include/qt5)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/contextsubscriber)
install(FILES include/contextsubscriber/contextproperty.h DESTINATION include)

endif(ENABLE_QT4)

add_subdirectory(src/util)
add_subdirectory(src/contextkit-subscriber)
add_subdirectory(tests)
add_subdirectory(tools/monitor)

MESSAGE(STATUS "QT4=${ENABLE_QT4}")
MESSAGE(STATUS "VERSION=${VERSION}")
