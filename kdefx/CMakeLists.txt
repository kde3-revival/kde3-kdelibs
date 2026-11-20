kde3_add_library(kdefx SHARED
  VERSION_INFO 6:0:2
  SOURCES
    kpixmap.cpp
    kpixmapsplitter.cpp
    kpixmapeffect.cpp
    kimageeffect.cpp
    kdrawutil.cpp
    kstyle.cpp
    kcpuinfo.cpp
  MOC
    kstyle.h
  LINK
    KDE3Qt::Qt
    kdemacros
  LINK_PRIVATE
    X11::Xrender
)
# all_includes: -I$(top_srcdir)/kdefx
target_include_directories(kdefx PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

install(FILES
  kpixmap.h
  kpixmapsplitter.h
  kpixmapeffect.h
  kimageeffect.h
  kdrawutil.h
  kstyle.h
  kcpuinfo.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})
