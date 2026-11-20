kde3_add_library(kresources SHARED
  VERSION_INFO 3:0:2
  SOURCES
    resource.cpp
    factory.cpp
    manageriface.skel
    manageriface.stub
    managerimpl.cpp
    configwidget.cpp
    configdialog.cpp
    selectdialog.cpp
    configpage.cpp
  MOC
    configdialog.h
    configpage.h
    configwidget.h
    resource.h
  LINK
    kdeui
    kio
)

kde3_add_module(kcm_kresources
  SOURCES
    kcmkresources.cpp
  MOC
    kcmkresources.h
  LINK
    kresources
)

install(FILES
  resource.h
  configwidget.h
  factory.h
  manageriface.h
  managerimpl.h
  manager.h
  selectdialog.h
  configdialog.h
  configpage.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kresources)

install(FILES
  kresources.desktop
  DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})

install(FILES
  kresources_plugin.desktop
  kresources_manager.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})
