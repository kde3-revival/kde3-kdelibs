add_subdirectory(kssl)
add_subdirectory(kio)
add_subdirectory(bookmarks)
add_subdirectory(kfile)

file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp" CONTENT "\n")
kde3_add_library(kio SHARED
  VERSION_INFO 6:0:2
  SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp
  LINK
    kdeui
    kdesu
    kwalletclient
  LINK_PRIVATE
    kssl
    kiocore
    ksycoca
    kbookmarks
    libkfile
)
target_compile_definitions(kio PRIVATE _LARGEFILE64_SOURCE)

install(FILES
  magic
  DESTINATION ${KDE3_MIME_INSTALL_DIR})

install(FILES
  application.desktop
  kurifilterplugin.desktop
  kcomprfilter.desktop
  kscan.desktop
  kdatatool.desktop
  kfileplugin.desktop
  kcmodule.desktop
  renamedlgplugin.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

install(FILES
  kioslave.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)
install(PROGRAMS
  useragent.pl
  proxytype.pl
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

install(FILES
  kpasswdserver.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)

install(FILES
  data.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

add_subdirectory(kpasswdserver)
add_subdirectory(misc)
add_subdirectory(pics)
add_subdirectory(kioexec)
add_subdirectory(httpfilter)
