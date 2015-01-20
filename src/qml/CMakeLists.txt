
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
install(FILES qmldir DESTINATION ${DST_LIB}/qt5/qml/Mer/State)
