# libksettings.la (convenience library of libkutils)
kde3_add_library(ksettings OBJECT
  SOURCES
    dispatcher.cpp
    dialog.cpp
    pluginpage.cpp
    componentsdialog.cpp
  MOC
    componentsdialog.h
    dialog.h
    dispatcher.h
    pluginpage.h
  LINK
    kio
)
target_include_directories(ksettings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")

install(FILES
  dispatcher.h
  dialog.h
  pluginpage.h
  componentsdialog.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/ksettings)

