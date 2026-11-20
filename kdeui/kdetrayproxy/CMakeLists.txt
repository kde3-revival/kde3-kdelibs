kde3_add_module(kded_kdetrayproxy
  SOURCES
    kdetrayproxy.cpp
    module.cpp
    module.skel
  MOC
    kdetrayproxy.h
    module.h
  LINK
    kdecore
)
target_include_directories(kded_kdetrayproxy PRIVATE "${PROJECT_SOURCE_DIR}/kded")

install(FILES
  kdetrayproxy.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)
