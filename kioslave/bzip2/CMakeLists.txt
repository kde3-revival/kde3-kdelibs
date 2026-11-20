kde3_add_module(kbzip2filter
  SOURCES
    kbzip2filter.cpp
  LINK
    kio
    BZip2::BZip2
)

install(FILES
  kbzip2filter.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(kbzip2filter PRIVATE "${PROJECT_SOURCE_DIR}/kio")
