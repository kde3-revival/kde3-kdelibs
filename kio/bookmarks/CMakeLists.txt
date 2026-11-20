# libkbookmarks.la (convenience library of libkio)
kde3_add_library(kbookmarks OBJECT
  SOURCES
    kbookmark.cc
    kbookmarkbar.cc
    kbookmarkdrag.cc
    kbookmarkexporter.cc
    kbookmarkimporter.cc
    kbookmarkmanager.cc
    kbookmarkmenu.cc
    kbookmarkimporter_crash.cc
    kbookmarkimporter_opera.cc
    kbookmarkimporter_ie.cc
    kbookmarkimporter_ns.cc
    kbookmarkimporter_kde1.cc
    kbookmarkdombuilder.cc
    kbookmarkmanager.skel
    kbookmarknotifier.skel
  MOC
    kbookmarkbar.h
    kbookmarkdombuilder.h
    kbookmarkimporter.h
    kbookmarkimporter_crash.h
    kbookmarkimporter_ie.h
    kbookmarkimporter_ns.h
    kbookmarkimporter_opera.h
    kbookmarkmanager.h
    kbookmarkmenu.h
    kbookmarkmenu_p.h
  LINK
    kdeui
    kiocore
)
target_include_directories(kbookmarks PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR}>")

install(FILES
  kbookmark.h
  kbookmarkbar.h
  kbookmarkdrag.h
  kbookmarkexporter.h
  kbookmarkimporter.h
  kbookmarkmanager.h
  kbookmarkmenu.h
  kbookmarknotifier.h
  kbookmarkimporter_crash.h
  kbookmarkimporter_opera.h
  kbookmarkimporter_ie.h
  kbookmarkimporter_ns.h
  kbookmarkimporter_kde1.h
  kbookmarkdombuilder.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})
