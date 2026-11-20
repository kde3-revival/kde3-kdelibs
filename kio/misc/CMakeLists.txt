kde3_add_kdeinit_executable(kio_uiserver
  SOURCES
    uiserver.cpp
    uiserver.skel
    ${PROJECT_SOURCE_DIR}/kio/kio/observer.stub
  MOC
    uiserver.h
  LINK
    kio
)
target_compile_definitions(kdeinit_kio_uiserver PRIVATE _LARGEFILE64_SOURCE)
target_include_directories(kdeinit_kio_uiserver PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.." "${PROJECT_SOURCE_DIR}/kio/kssl"
  "${PROJECT_BINARY_DIR}/kio/kssl")

kde3_add_executable(kmailservice
  SOURCES
    kmailservice.cpp
  LINK
    kdecore
)

kde3_add_executable(ktelnetservice
  SOURCES
    ktelnetservice.cpp
  LINK
    kdeui
)

install(FILES
  kio_uiserver.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

install(FILES
  kmailservice.protocol
  telnet.protocol
  rlogin.protocol
  ssh.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

install(FILES
  uiserver.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kio)

install(PROGRAMS
  fileshareset
  DESTINATION ${KDE3_BIN_INSTALL_DIR})
# install-exec-local
kde3_install_symlink(fileshareset ${KDE3_BIN_INSTALL_DIR}/filesharelist)
kde3_install_setuid_root(fileshareset)

add_subdirectory(ksendbugmail)
add_subdirectory(kpac)
add_subdirectory(kssld)
add_subdirectory(kfile)
add_subdirectory(kwalletd)
add_subdirectory(kntlm)
