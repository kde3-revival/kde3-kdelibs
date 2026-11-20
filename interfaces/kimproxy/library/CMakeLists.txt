kde3_add_library(kimproxy SHARED
  VERSION_INFO 0:0:0
  SOURCES
    kimproxy.cpp
    ${PROJECT_SOURCE_DIR}/interfaces/kimproxy/interface/kimiface.stub
    kimproxyiface.skel
  MOC
    kimproxy.h
  LINK
    kio
)

install(FILES
  kimproxy.h
  kimproxyiface.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

add_subdirectory(icons)
