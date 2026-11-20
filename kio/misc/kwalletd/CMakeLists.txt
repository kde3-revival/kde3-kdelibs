kde3_add_module(kded_kwalletd
  SOURCES
    kwalletd.cpp
    kwalletd.skel
    ktimeout.cpp
    kwalletwizard.ui
    kbetterthankdialogbase.ui
  MOC
    ktimeout.h
    kwalletd.h
  LINK
    kio
    kwalletbackend
    kwalletclient
)
target_include_directories(kded_kwalletd PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${PROJECT_SOURCE_DIR}/kwallet/backend"
  "${PROJECT_SOURCE_DIR}/kwallet/client")

install(FILES
  kwalletd.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)
