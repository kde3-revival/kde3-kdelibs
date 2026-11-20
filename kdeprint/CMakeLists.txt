# KDE_OPTIONS = nofinal; the whole directory is compiled with
# -D_KDEPRINT_COMPILE and INCLUDES = -I$(top_srcdir)/kded
add_compile_definitions(_KDEPRINT_COMPILE)
include_directories("${PROJECT_SOURCE_DIR}/kded")

# noinst_DATA = kdeprint: "kdeprint" -> management in the build
# directory, for <kdeprint/...> includes of the management headers
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/management"
  "${CMAKE_CURRENT_BINARY_DIR}/kdeprint" SYMBOLIC)

kde3_add_module(kded_kdeprintd
  SOURCES
    kdeprintd.skel
    kdeprintd.cpp
    kprintprocess.cpp
  MOC
    kdeprintd.h
    kprintprocess.h
  LINK
    kio
)

kde3_add_library(kdeprint SHARED
  VERSION_INFO 6:0:2
  SOURCES
    kmfactory.cpp
    kmjob.cpp
    kmjobmanager.cpp
    kmmanager.cpp
    kmprinter.cpp
    kmuimanager.cpp
    kmvirtualmanager.cpp
    driver.cpp
    driveritem.cpp
    droptionview.cpp
    driverview.cpp
    kmdbentry.cpp
    kprintpreview.cpp
    kprintdialog.cpp
    kprinter.cpp
    kprinterimpl.cpp
    kprintdialogpage.cpp
    kpcopiespage.cpp
    kpdriverpage.cpp
    kprinterpropertydialog.cpp
    kpgeneralpage.cpp
    kpqtpage.cpp
    kmthreadjob.cpp
    kpipeprocess.cpp
    kpfilterpage.cpp
    kmspecialmanager.cpp
    kdeprintcheck.cpp
    plugincombobox.cpp
    kpreloadobject.cpp
    kfilelist.cpp
    kprintaction.cpp
    kpfileselectpage.cpp
    treecombobox.cpp
    kmfactory.skel
    kxmlcommand.cpp
    util.cpp
    printerfilter.cpp
    marginpreview.cpp
    marginvaluewidget.cpp
    marginwidget.cpp
    kpmarginpage.cpp
    messagewindow.cpp
    posterpreview.cpp
    kpposterpage.cpp
    foomatic2loader.cpp
    fooparser.cpp
    fooscanner.cpp
    ppdparser.cpp
    ppdscanner.cpp
    ppdloader.cpp
  MOC
    driverview.h
    droptionview.h
    kfilelist.h
    kmfactory.h
    kmjobmanager.h
    kmmanager.h
    kmuimanager.h
    kpcopiespage.h
    kpfilterpage.h
    kpgeneralpage.h
    kpposterpage.h
    kpqtpage.h
    kprintaction.h
    kprintdialog.h
    kprintdialogpage.h
    kprinterimpl.h
    kprinterpropertydialog.h
    kprintpreview.h
    marginpreview.h
    marginvaluewidget.h
    marginwidget.h
    messagewindow.h
    plugincombobox.h
    posterpreview.h
  LINK
    kparts
  LINK_PRIVATE
    ZLIB::ZLIB
)

install(FILES
  kprinter.h
  kprintaction.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

install(FILES
  kprintdialogpage.h
  kpreloadobject.h
  kmmanager.h
  kmprinter.h
  kmobject.h
  kprintdialog.h
  driver.h
  kmjobmanager.h
  kmjob.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kdeprint)

install(FILES
  kprintpreviewui.rc
  DESTINATION ${KDE3_CONFIG_INSTALL_DIR}/ui)

install(FILES
  testprint.ps
  specials.desktop
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kdeprint)

install(FILES
  kdeprintd.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kded)

install(FILES
  kdeprintrc
  DESTINATION ${KDE3_CONFIG_INSTALL_DIR})

install(FILES
  kdeprintrc.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

# libdriverparse.a (noinst_LIBRARIES) for make_driver_db_cups
add_library(driverparse STATIC driverparse.c)

add_subdirectory(management)
add_subdirectory(cups)
add_subdirectory(filters)
add_subdirectory(pics)
