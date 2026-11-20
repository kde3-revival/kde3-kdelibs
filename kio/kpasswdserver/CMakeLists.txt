kde3_add_module(kded_kpasswdserver
  SOURCES
    kpasswdserver.cpp
    kpasswdserver.skel
  MOC
    kpasswdserver.h
  LINK
    kwalletclient
    kio
)
target_include_directories(kded_kpasswdserver PRIVATE "${PROJECT_SOURCE_DIR}/kwallet/client")
