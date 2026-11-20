kde3_add_module(libkmultipart
  SOURCES
    kmultipart.cpp
  MOC
    kmultipart.h
  LINK
    kparts
    httpfilter
)
target_include_directories(libkmultipart PRIVATE "${PROJECT_SOURCE_DIR}/kio/httpfilter")

install(FILES
  kmultipart.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
