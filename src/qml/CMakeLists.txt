
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC TRUE)

find_package(Qt5Qml REQUIRED)

add_library(statefs-declarative
  SHARED
  plugin.cpp property.cpp
)
qt5_use_modules(statefs-declarative Qml)
target_link_libraries(statefs-declarative
  contextkit-statefs-qt5
)

set_target_properties(statefs-declarative PROPERTIES
  SOVERSION 0
  VERSION ${VERSION}
  )

install(TARGETS statefs-declarative DESTINATION ${DST_LIB}/qt5/qml/Mer/State)
install(FILES qmldir plugins.qmltypes DESTINATION ${DST_LIB}/qt5/qml/Mer/State)

add_library(contextkit
  SHARED
  contextkit_plugin.cpp contextkit_property.cpp
)
qt5_use_modules(contextkit Qml)
target_link_libraries(contextkit
  contextkit-statefs-qt5
)

install(TARGETS contextkit DESTINATION ${DST_LIB}/qt5/qml/Nemo/StateFs)
install(FILES contextkit-qmldir
  DESTINATION ${DST_LIB}/qt5/qml/Nemo/StateFs
  RENAME qmldir)
install(FILES contextkit-plugins.qmltypes
  DESTINATION ${DST_LIB}/qt5/qml/Nemo/StateFs
  RENAME plugins.qmltypes)

add_custom_target(qmltypes
  COMMAND qmlplugindump -nonrelocatable Mer.State 1.1 > plugins.qmltypes
  COMMAND qmlplugindump -nonrelocatable Nemo.StateFs 1.0 > contextkit-plugins.qmltypes
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
