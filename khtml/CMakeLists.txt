# INCLUDES of the khtml directories; $(top_builddir)/kjs (global.h) must
# come before kio/kio of all_includes
include_directories(BEFORE "${PROJECT_BINARY_DIR}/kjs")
include_directories(
  "${PROJECT_SOURCE_DIR}/kjs"
  "${PROJECT_SOURCE_DIR}/kimgio"
  "${PROJECT_SOURCE_DIR}/khtml"
  "${PROJECT_SOURCE_DIR}/khtml/misc"
  "${PROJECT_SOURCE_DIR}/khtml/dom"
  "${PROJECT_SOURCE_DIR}/khtml/xml"
  "${PROJECT_SOURCE_DIR}/khtml/html"
  "${PROJECT_SOURCE_DIR}/khtml/css"
  "${PROJECT_SOURCE_DIR}/kio/kssl"
  "${PROJECT_BINARY_DIR}/kio/kssl"
  "${PROJECT_SOURCE_DIR}/kio/bookmarks"
  "${PROJECT_SOURCE_DIR}/kdeprint"
  "${PROJECT_SOURCE_DIR}/interfaces"
  "${PROJECT_SOURCE_DIR}/interfaces/kregexpeditor"
  "${PROJECT_SOURCE_DIR}/kwallet/client"
  "${PROJECT_SOURCE_DIR}/kutils")

add_subdirectory(misc)
add_subdirectory(dom)
add_subdirectory(css)
add_subdirectory(xml)
add_subdirectory(html)
add_subdirectory(rendering)
add_subdirectory(pics)
add_subdirectory(ecma)

kde3_add_library(khtml SHARED
  VERSION_INFO 6:0:2
  SOURCES
    kjserrordlg.ui
    khtmlview.cpp
    khtml_part.cpp
    khtml_run.cpp
    khtml_factory.cpp
    khtml_settings.cc
    khtml_events.cpp
    khtml_ext.cpp
    khtml_pagecache.cpp
    khtml_iface.skel
    khtml_iface.cc
    khtml_printsettings.cpp
    htmlpageinfo.ui
    khtmlimage.cpp
  MOC
    khtml_ext.h
    khtml_factory.h
    khtml_pagecache.h
    khtml_part.h
    khtml_printsettings.h
    khtml_run.h
    khtmlimage.h
    khtmlpart_p.h
    khtmlview.h
  LINK
    kparts
    kdeprint
    kutils
    kwalletclient
  LINK_PRIVATE
    khtmlxml
    khtmlhtml
    khtmlrender
    khtmlcss
    khtmlmisc
    kjs_html
    khtmldom
)
if(HAVE_LIBTHAI)
  target_link_libraries(khtml PRIVATE PkgConfig::LIBTHAI)
endif()
# include_VERSION_SCRIPT (GNU ld)
target_link_options(khtml PRIVATE
  "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libkhtml.map")
set_property(TARGET khtml APPEND PROPERTY LINK_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/libkhtml.map")

# libkhtmlpart and khtmlimagepart: init_ functions forwarding to libkhtml
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp" CONTENT
"#include <kdelibs_export.h>
extern \"C\" KDE_EXPORT void* init_libkhtml();
extern \"C\" KDE_EXPORT void* init_libkhtmlpart() { return init_libkhtml(); } 
")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/imgdummy.cpp" CONTENT
"#include <kdelibs_export.h>
extern \"C\"  KDE_EXPORT void* init_khtmlimagefactory();
extern \"C\"  KDE_EXPORT void* init_khtmlimagepart() { return init_khtmlimagefactory(); } 
")

kde3_add_module(libkhtmlpart
  SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp
  LINK
    khtml
)

kde3_add_module(khtmlimagepart
  SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/imgdummy.cpp
  LINK
    khtml
)

install(FILES
  khtmlview.h
  khtml_part.h
  khtml_events.h
  khtml_settings.h
  khtmldefaults.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

install(FILES
  khtml.desktop
  khtmlimage.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

install(FILES
  khtml.rc
  khtml_browser.rc
  khtml_popupmenu.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/khtml)

add_subdirectory(kmultipart)
