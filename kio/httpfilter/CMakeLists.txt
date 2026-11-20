# libhttpfilter.la (convenience library of kio_http)
kde3_add_library(httpfilter OBJECT
  SOURCES
    httpfilter.cc
  MOC
    httpfilter.h
  LINK
    kdecore
    ZLIB::ZLIB
)
