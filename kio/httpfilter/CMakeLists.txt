# libhttpfilter.la (convenience library of kio_http)
kde3_add_library(httpfilter OBJECT
  SOURCES
    httpfilter.cc
  MOC
    httpfilter.h
  LINK
    kdecore
    ZLIB::ZLIB
    PkgConfig::LIBZSTD
)
if(HAVE_LIBBROTLIDEC)
  target_link_libraries(httpfilter PUBLIC PkgConfig::LIBBROTLIDEC)
endif()
