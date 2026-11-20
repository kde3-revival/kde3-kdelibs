kde3_add_module(kgzipfilter
  SOURCES
    kgzipfilter.cpp
  LINK
    kio
    ZLIB::ZLIB
)

install(FILES
  kgzipfilter.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(kgzipfilter PRIVATE "${PROJECT_SOURCE_DIR}/kio")
