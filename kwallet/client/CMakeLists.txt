kde3_add_library(kwalletclient SHARED
  VERSION_INFO 1:1
  SOURCES
    kwallet.skel
    kwallet.cc
  MOC
    kwallet.h
  LINK
    kdecore
)

install(FILES
  kwallet.h
  kwallettypes.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})
