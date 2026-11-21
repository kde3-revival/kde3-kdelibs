kde3_add_kdeinit_executable(kded
  SOURCES
    kded.cpp
    kdedmodule.cpp
  MOC
    kded.h
    kdedmodule.h
  LINK
    kio
)

kde3_add_kdeinit_executable(kbuildsycoca
  SOURCES
    kbuildsycoca.cpp
    kbuildservicetypefactory.cpp
    kbuildservicefactory.cpp
    kbuildservicegroupfactory.cpp
    kbuildimageiofactory.cpp
    kbuildprotocolinfofactory.cpp
    kctimefactory.cpp
    ksharedmimeinfo.cpp
    vfolder_menu.cpp
  MOC
    kbuildsycoca.h
    vfolder_menu.h
  LINK
    kio
)

kde3_add_executable(kdontchangethehostname
  SOURCES
    khostname.cpp
  LINK
    kdecore
)

kde3_add_executable(kde-menu
  SOURCES
    kde-menu.cpp
  LINK
    kio
)

install(FILES
  kdedmodule.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

install(FILES
  kdedmodule.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

install(FILES
  applications.menu
  DESTINATION ${KDE3_XDG_MENU_INSTALL_DIR})

install(FILES
  kded.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

foreach(_t kdeinit_kded kdeinit_kbuildsycoca)
  target_include_directories(${_t} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
endforeach()
