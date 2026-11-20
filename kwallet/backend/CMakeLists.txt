kde3_add_library(kwalletbackend SHARED
  VERSION_INFO 1:0
  SOURCES
    blockcipher.cc
    blowfish.cc
    cbc.cc
    sha1.cc
    kwalletentry.cc
    kwalletbackend.cc
  LINK
    KDE3Qt::Qt
    kdecore
)
target_include_directories(kwalletbackend PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../client")
