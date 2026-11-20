# libkdecorenetwork.la (convenience library of libkdecore)
kde3_add_library(kdecorenetwork OBJECT
  SOURCES
    kresolver.cpp
    kresolvermanager.cpp
    kresolverworkerbase.cpp
    ksocketaddress.cpp
    kresolverstandardworkers.cpp
    kreverseresolver.cpp
    ksocketdevice.cpp
    ksocketbase.cpp
    kclientsocketbase.cpp
    kstreamsocket.cpp
    kserversocket.cpp
    kdatagramsocket.cpp
    khttpproxysocketdevice.cpp
    ksockssocketdevice.cpp
    kbufferedsocket.cpp
    ksocketbuffer.cpp
    ksrvresolverworker.cpp
  MOC
    kbufferedsocket.h
    kclientsocketbase.h
    kdatagramsocket.h
    kresolver.h
    kreverseresolver.h
    kserversocket.h
    ksrvresolverworker_p.h
    kstreamsocket.h
  LINK
    DCOP
    kdefx
    kdemacros
)
target_include_directories(kdecorenetwork PRIVATE "${PROJECT_SOURCE_DIR}/kdecore")
if(HAVE_LIBIDN)
  target_link_libraries(kdecorenetwork PRIVATE PkgConfig::LIBIDN)
endif()

install(FILES
  kresolver.h
  kreverseresolver.h
  ksocketaddress.h
  ksocketbase.h
  ksocketdevice.h
  kclientsocketbase.h
  kstreamsocket.h
  kserversocket.h
  kdatagramsocket.h
  kmulticastsocketdevice.h
  kmulticastsocket.h
  knetworkinterface.h
  khttpproxysocketdevice.h
  ksockssocketdevice.h
  kbufferedsocket.h
  kiobuffer.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

install(FILES
  ipv6blacklist
  DESTINATION ${KDE3_CONFIG_INSTALL_DIR})
