configure_file(ksslconfig.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/ksslconfig.h")

# libkssl.la (convenience library of libkio)
kde3_add_library(kssl OBJECT
  SOURCES
    kssl.cc
    ksslcertificatehome.cc
    ksslsettings.cc
    ksslcertificate.cc
    ksslconnectioninfo.cc
    ksslcertificatefactory.cc
    ksslcertificatecache.cc
    ksslpeerinfo.cc
    ksslinfodlg.cc
    ksslcertdlg.cc
    ksslutils.cc
    kopenssl.cc
    ksslpkcs12.cc
    ksslcertchain.cc
    ksslkeygen.cc
    ksslx509v3.cc
    ksslsigners.cc
    ksslpkcs7.cc
    ksslpemcallback.cc
    ksslx509map.cc
    ksslsession.cc
    keygenwizard.ui
    keygenwizard2.ui
    ksmimecrypto.cc
    ksslcsessioncache.cc
  MOC
    ksslcertdlg.h
    ksslinfodlg.h
    ksslkeygen.h
  LINK
    kdeui
)
target_include_directories(kssl PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR}>")
target_include_directories(kssl PRIVATE "${PROJECT_SOURCE_DIR}/kwallet/client")
if(HAVE_LIBPSL)
  target_link_libraries(kssl PRIVATE PkgConfig::LIBPSL)
endif()
target_link_libraries(kssl PRIVATE OpenSSL::SSL OpenSSL::Crypto)
# installed kssl headers include OpenSSL headers: those of the OpenSSL
# linked, also for the sources of libkio that include them
target_include_directories(kssl PUBLIC "$<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}>")

install(FILES
  kssl.h
  ksslcertificatehome.h
  ksslsettings.h
  ksslcertificate.h
  ksslconnectioninfo.h
  ksslcertificatefactory.h
  ksslcertificatecache.h
  ksslpeerinfo.h
  ksslinfodlg.h
  ksslcertdlg.h
  ksslutils.h
  kopenssl.h
  ksslall.h
  ksslpkcs12.h
  ksslcertchain.h
  ksslkeygen.h
  ksslx509v3.h
  ksslsigners.h
  ksslpkcs7.h
  ksslpemcallback.h
  ${CMAKE_CURRENT_BINARY_DIR}/ksslconfig.h
  ksslsession.h
  ksslx509map.h
  ksmimecrypto.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})
