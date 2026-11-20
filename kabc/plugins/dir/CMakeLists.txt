kde3_add_library(kabc_dir SHARED
  VERSION_INFO 1:0:0
  SOURCES
    resourcedir.cpp
    resourcedirconfig.cpp
  MOC
    resourcedir.h
    resourcedirconfig.h
  LINK
    kabc
)

kde3_add_module(kabc_dir_plugin
  OUTPUT_NAME kabc_dir
  SOURCES
    resourcedirplugin.cpp
  LINK
    kabc_dir
)

install(FILES
  resourcedir.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kabc)

install(FILES
  dir.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kabc)
