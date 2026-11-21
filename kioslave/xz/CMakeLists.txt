kde3_add_module(kxzfilter
  SOURCES
    kxzfilter.cpp
  LINK
    kio
    PkgConfig::LIBLZMA
)

install(FILES
  kxzfilter.desktop
  klzmafilter.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(kxzfilter PRIVATE "${PROJECT_SOURCE_DIR}/kio")
