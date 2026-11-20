add_subdirectory(ksettings)

kde3_add_library(kutils SHARED
  VERSION_INFO 3:0:2
  SOURCES
    kfind.cpp
    kfinddialog.cpp
    kreplace.cpp
    kreplacedialog.cpp
    kcmoduleinfo.cpp
    kcmoduleloader.cpp
    kcmultidialog.cpp
    kmultitabbar.cpp
    kplugininfo.cpp
    kcmoduleproxy.cpp
    kpluginselector.cpp
    kcmodulecontainer.cpp
    kcmoduleproxyIface.skel
    kcmoduleproxyIfaceImpl.cpp
  MOC
    kcmodulecontainer.h
    kcmoduleproxy.h
    kcmoduleproxyIfaceImpl.h
    kcmultidialog.h
    kfind.h
    kfinddialog.h
    kmultitabbar.h
    kmultitabbar_p.h
    kpluginselector.h
    kpluginselector_p.h
    kreplace.h
    kreplacedialog.h
  LINK
    kio
  LINK_PRIVATE
    ksettings
)
target_include_directories(kutils PRIVATE "${PROJECT_SOURCE_DIR}/interfaces/kregexpeditor")

install(FILES
  kfind.h
  kfinddialog.h
  kreplace.h
  kreplacedialog.h
  kcmoduleinfo.h
  kcmoduleloader.h
  kcmultidialog.h
  kmultitabbar.h
  kplugininfo.h
  kcmoduleproxy.h
  kpluginselector.h
  kcmodulecontainer.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

install(FILES
  kplugininfo.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

