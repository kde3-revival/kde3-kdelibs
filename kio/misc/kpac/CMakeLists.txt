kde3_add_module(kded_proxyscout
  EXCEPTIONS
  SOURCES
    proxyscout.skel
    proxyscout.cpp
    script.cpp
    downloader.cpp
    discovery.cpp
  MOC
    discovery.h
    downloader.h
    proxyscout.h
  LINK
    kio
    kjs
)
target_include_directories(kded_proxyscout PRIVATE "${PROJECT_SOURCE_DIR}/kded")
# -I$(top_builddir)/kjs before all_includes: kjs's generated global.h,
# not kio/kio/global.h
target_include_directories(kded_proxyscout BEFORE PRIVATE "${PROJECT_BINARY_DIR}/kjs")

kde3_add_executable(kpac_dhcp_helper
  SOURCES
    kpac_dhcp_helper.c
)
set_target_properties(kpac_dhcp_helper PROPERTIES POSITION_INDEPENDENT_CODE ON)
# install-exec-local
kde3_install_setuid_root(kpac_dhcp_helper)

install(FILES
  proxyscout.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)

install(FILES
  eventsrc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/proxyscout)
