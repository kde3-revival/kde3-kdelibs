kde3_add_library(kdnssd SHARED
  VERSION_INFO 1:0
  SOURCES
    remoteservice.cpp
    responder.cpp
    servicebase.cpp
    settings.kcfgc
    publicservice.cpp
    query.cpp
    domainbrowser.cpp
    servicebrowser.cpp
  MOC
    domainbrowser.h
    publicservice.h
    query.h
    remoteservice.h
    responder.h
    servicebrowser.h
  LINK
    kdecore
)
if(HAVE_DNSSD)
  target_link_libraries(kdnssd PRIVATE ${DNSSD_LIBRARY})
  target_include_directories(kdnssd PRIVATE ${DNSSD_INCLUDE_DIR})
endif()

install(FILES
  domainbrowser.h
  query.h
  remoteservice.h
  publicservice.h
  servicebase.h
  servicebrowser.h
  ${CMAKE_CURRENT_BINARY_DIR}/settings.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/dnssd)
