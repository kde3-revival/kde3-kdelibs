kde3_add_library(kparts SHARED
  VERSION_INFO 3:0:1
  SOURCES
    part.cpp
    plugin.cpp
    partmanager.cpp
    mainwindow.cpp
    dockmainwindow.cpp
    event.cpp
    browserextension.cpp
    factory.cpp
    historyprovider.cpp
    browserinterface.cpp
    browserrun.cpp
    statusbarextension.cpp
  MOC
    browserextension.h
    browserinterface.h
    browserrun.h
    dockmainwindow.h
    factory.h
    historyprovider.h
    mainwindow.h
    part.h
    partmanager.h
    plugin.h
    statusbarextension.h
  LINK
    kio
)

install(FILES
  part.h
  plugin.h
  partmanager.h
  mainwindow.h
  dockmainwindow.h
  event.h
  browserextension.h
  factory.h
  historyprovider.h
  browserinterface.h
  genericfactory.h
  componentfactory.h
  browserrun.h
  statusbarextension.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kparts)

install(FILES
  kpart.desktop
  krop.desktop
  krwp.desktop
  browserview.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

