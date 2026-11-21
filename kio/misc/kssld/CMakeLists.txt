kde3_add_module(kded_kssld
  SOURCES
    kssld.cpp
    kssld.skel
  MOC
    kssld.h
  LINK
    kio
)
target_include_directories(kded_kssld PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.." "${PROJECT_SOURCE_DIR}/kio/kssl"
  "${PROJECT_BINARY_DIR}/kio/kssl")
target_include_directories(kded_kssld PRIVATE ${OPENSSL_INCLUDE_DIR})

install(FILES
  kssld.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)
