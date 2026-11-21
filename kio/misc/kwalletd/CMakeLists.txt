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
# the Secret Service backend (the GNOME login keyring)
if(HAVE_LIBSECRET)
  target_sources(kded_kwalletd PRIVATE kwalletsecretservice.cpp)
  target_link_libraries(kded_kwalletd PRIVATE PkgConfig::LIBSECRET)
endif()

target_include_directories(kded_kwalletd PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${PROJECT_SOURCE_DIR}/kwallet/backend"
  "${PROJECT_SOURCE_DIR}/kwallet/client")

install(FILES
  kwalletd.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)

# tests/run-tests.sh: the Secret Service backend against a private keyring
if(HAVE_LIBSECRET AND KDE3_BUILD_KWALLETD_TESTS)
  add_subdirectory(tests)
endif()
