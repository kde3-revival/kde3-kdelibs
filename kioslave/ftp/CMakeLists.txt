kde3_add_module(kio_ftp
  SOURCES
    ftp.cc
  LINK
    kio
)

install(FILES
  ftp.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
