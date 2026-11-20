kde3_add_library(kabc_ldapkio SHARED
  VERSION_INFO 1:0:0
  SOURCES
    resourceldapkio.cpp
    resourceldapkioconfig.cpp
  MOC
    resourceldapkio.h
    resourceldapkioconfig.h
  LINK
    kabc
    kio
)

kde3_add_module(kabc_ldapkio_plugin
  OUTPUT_NAME kabc_ldapkio
  SOURCES
    resourceldapkioplugin.cpp
  LINK
    kabc_ldapkio
)

install(FILES
  resourceldapkio.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kabc)

install(FILES
  ldapkio.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kabc)
