kde3_add_module(kio_metainfo
  SOURCES
    metainfo.cpp
  LINK
    kio
)

install(FILES
  metainfo.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
