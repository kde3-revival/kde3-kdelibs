kde3_add_library(kabc_file SHARED
  VERSION_INFO 1:0:0
  SOURCES
    resourcefile.cpp
    resourcefileconfig.cpp
  MOC
    resourcefile.h
    resourcefileconfig.h
  LINK
    kabc
)

kde3_add_module(kabc_file_plugin
  OUTPUT_NAME kabc_file
  SOURCES
    resourcefileplugin.cpp
  LINK
    kabc_file
)

install(FILES
  resourcefile.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kabc)

install(FILES
  file.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kabc)
