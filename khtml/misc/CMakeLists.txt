# libkhtmlmisc.la (convenience library of libkhtml)
kde3_add_library(khtmlmisc OBJECT
  LINK
    kparts
    kutils
    kwalletclient
    JPEG::JPEG
  SOURCES
    decoder.cpp
    loader.cpp
    loader_jpeg.cpp
    guess_ja.cpp
    htmlhashes.cpp
    helper.cpp
    arena.cpp
    stringit.cpp
  MOC
    loader.h
)
