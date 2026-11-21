kde3_add_module(kzstdfilter
  SOURCES
    kzstdfilter.cpp
  LINK
    kio
    PkgConfig::LIBZSTD
)

install(FILES
  kzstdfilter.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(kzstdfilter PRIVATE "${PROJECT_SOURCE_DIR}/kio")
