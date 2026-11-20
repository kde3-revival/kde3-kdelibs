kde3_add_kdeinit_executable(kio_http_cache_cleaner
  SOURCES
    http_cache_cleaner.cpp
  LINK
    kio
)

kde3_add_module(kio_http
  SOURCES
    http.cc
  MOC
    http.h
  LINK
    kio
    httpfilter
    kntlm
)

install(FILES
  http_cache_cleaner.desktop
  http.protocol
  https.protocol
  webdav.protocol
  webdavs.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(kio_http PRIVATE
  "${PROJECT_SOURCE_DIR}/interfaces" "${PROJECT_SOURCE_DIR}/kio/httpfilter")
if(HAVE_LIBGSSAPI)
  target_link_libraries(kio_http PRIVATE PkgConfig::GSSAPI)
endif()

add_subdirectory(kcookiejar)
