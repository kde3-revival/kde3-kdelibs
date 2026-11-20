kde3_add_kdeinit_executable(kcookiejar
  SOURCES
    main.cpp
  LINK
    kdecore
)

kde3_add_module(kded_kcookiejar
  SOURCES
    kcookiejar.cpp
    kcookieserver.cpp
    kcookieserver.skel
    kcookiewin.cpp
  MOC
    kcookieserver.h
    kcookiewin.h
  LINK
    kio
)

install(FILES
  kcookiejar.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)

install(FILES
  kcookiescfg.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

install(FILES
  domain_info
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/khtml)
