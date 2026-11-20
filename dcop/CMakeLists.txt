add_subdirectory(KDE-ICE)

configure_file(dcop-path.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/dcop-path.h" @ONLY)

# KDE_OPTIONS = qtonly
kde3_add_library(DCOP SHARED
  VERSION_INFO 6:0:2
  SOURCES
    dcopstub.cpp
    dcopref.cpp
    dcopobject.cpp
    dcopclient.cpp
  MOC
    dcopclient.h
  LINK
    KDE3Qt::Qt
    kdemacros
  LINK_PRIVATE
    kICE
)
# all_includes: -I$(top_srcdir)/dcop
target_include_directories(DCOP PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

kde3_add_kdeinit_executable(dcopserver
  SOURCES
    dcopserver.cpp
    dcopsignals.cpp
  MOC
    dcopserver.h
  LINK
    DCOP
)

kde3_add_executable(dcopserver_shutdown
  SOURCES
    dcopserver_shutdown.c
  LINK
    DCOP
)

install(FILES
  dcopclient.h
  dcopobject.h
  dcopstub.h
  dcopref.h
  dcoptypes.h
  kdatastream.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

add_subdirectory(dcopidl)
add_subdirectory(dcopidlng)
add_subdirectory(dcopidl2cpp)
add_subdirectory(client)
