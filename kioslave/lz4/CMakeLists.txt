kde3_add_module(klz4filter
  SOURCES
    klz4filter.cpp
  LINK
    kio
    PkgConfig::LIBLZ4
)

install(FILES
  klz4filter.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(klz4filter PRIVATE "${PROJECT_SOURCE_DIR}/kio")
