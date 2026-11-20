#[=======================================================================[.rst:
FindKDE3Qt
----------

Finds kde3-qt, the Qt 3 of the KDE 3 revival (libkde3-qt.so.3,
libkde3-qui.so.1, moc and uic).  A stock Qt 3 (libqt-mt) or any other Qt
is rejected: kde3-qt marks itself with a QT_BUILD_KEY starting with
"kde3-revival".

Search order:

* ``QT_DIR`` (CMake or environment variable) or ``QTDIR`` (environment):
  the kde3-qt build or install tree (``include/``, ``lib/``, ``bin/``);
  nothing else is searched when it is set,
* the ``kde3-qt`` pkg-config module,
* ``/opt/kde3-qt``,
* ``KDE3Qt_FALLBACK_DIR`` if set (KDE3LibsConfig sets it to the kde3-qt
  kdelibs was built with, so that it is only used when none of the
  above has one).

Imported targets:

``KDE3Qt::Qt``
  libkde3-qt (threaded), with the include directory, the defines KDE
  compiles Qt code with (QT_THREAD_SUPPORT, _REENTRANT) and the X11 and
  thread libraries of autotools' LIB_QT.
``KDE3Qt::QUI``
  libkde3-qui (QWidgetFactory).
``KDE3Qt::moc``, ``KDE3Qt::uic``
  The code generators.

Result variables: ``KDE3Qt_FOUND``, ``KDE3Qt_VERSION``,
``KDE3Qt_INCLUDE_DIR``, ``KDE3Qt_LIBRARY_DIR``, ``KDE3Qt_LIBRARY``,
``KDE3Qt_QUI_LIBRARY``, ``KDE3Qt_MOC_EXECUTABLE``,
``KDE3Qt_UIC_EXECUTABLE``.
#]=======================================================================]

include(FindPackageHandleStandardArgs)

set(_kde3qt_hints)
set(_kde3qt_search NO_DEFAULT_PATH)
if(QT_DIR)
  list(APPEND _kde3qt_hints "${QT_DIR}")
elseif(DEFINED ENV{QT_DIR})
  list(APPEND _kde3qt_hints "$ENV{QT_DIR}")
elseif(DEFINED ENV{QTDIR})
  list(APPEND _kde3qt_hints "$ENV{QTDIR}")
else()
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(_KDE3QT_PC QUIET kde3-qt)
    if(_KDE3QT_PC_FOUND)
      pkg_get_variable(_kde3qt_pc_prefix kde3-qt prefix)
      list(APPEND _kde3qt_hints "${_kde3qt_pc_prefix}")
    endif()
  endif()
  list(APPEND _kde3qt_hints /opt/kde3-qt ${KDE3Qt_FALLBACK_DIR})
endif()

find_path(KDE3Qt_INCLUDE_DIR NAMES qglobal.h qconfig.h
  HINTS ${_kde3qt_hints} PATH_SUFFIXES include ${_kde3qt_search})
find_library(KDE3Qt_LIBRARY NAMES kde3-qt
  HINTS ${_kde3qt_hints} PATH_SUFFIXES lib lib64 ${_kde3qt_search})
find_library(KDE3Qt_QUI_LIBRARY NAMES kde3-qui
  HINTS ${_kde3qt_hints} PATH_SUFFIXES lib lib64 ${_kde3qt_search})
find_program(KDE3Qt_MOC_EXECUTABLE NAMES moc
  HINTS ${_kde3qt_hints} PATH_SUFFIXES bin ${_kde3qt_search})
find_program(KDE3Qt_UIC_EXECUTABLE NAMES uic
  HINTS ${_kde3qt_hints} PATH_SUFFIXES bin ${_kde3qt_search})

set(_kde3qt_is_kde3qt FALSE)
if(KDE3Qt_INCLUDE_DIR AND EXISTS "${KDE3Qt_INCLUDE_DIR}/qglobal.h"
   AND EXISTS "${KDE3Qt_INCLUDE_DIR}/qconfig.h")
  file(STRINGS "${KDE3Qt_INCLUDE_DIR}/qglobal.h" _kde3qt_ver
    REGEX "^#define[ \t]+QT_VERSION_STR[ \t]+\"[^\"]+\"")
  string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" KDE3Qt_VERSION "${_kde3qt_ver}")
  file(STRINGS "${KDE3Qt_INCLUDE_DIR}/qconfig.h" _kde3qt_key
    REGEX "^#define[ \t]+QT_BUILD_KEY[ \t]+\"kde3-revival")
  if(_kde3qt_key)
    set(_kde3qt_is_kde3qt TRUE)
  elseif(NOT KDE3Qt_FIND_QUIETLY)
    message(WARNING "${KDE3Qt_INCLUDE_DIR} is not kde3-qt "
      "(QT_BUILD_KEY in qconfig.h does not start with \"kde3-revival\")")
  endif()
endif()

find_package_handle_standard_args(KDE3Qt
  REQUIRED_VARS KDE3Qt_LIBRARY KDE3Qt_INCLUDE_DIR KDE3Qt_MOC_EXECUTABLE
                KDE3Qt_UIC_EXECUTABLE _kde3qt_is_kde3qt
  VERSION_VAR KDE3Qt_VERSION)

if(KDE3Qt_FOUND)
  get_filename_component(KDE3Qt_LIBRARY_DIR "${KDE3Qt_LIBRARY}" DIRECTORY)

  if(NOT TARGET KDE3Qt::Qt)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(PNG REQUIRED)

    # autotools: LIB_QT = -lqt-mt $(LIBZ) $(LIBPNG) -lXext $(LIB_X11)
    # $(LIBSM) -lpthread, all_includes contains $(QT_INCLUDES) and
    # $(X_INCLUDES), CPPFLAGS has -DQT_THREAD_SUPPORT and (Linux)
    # -D_REENTRANT, FreeBSD -D_THREAD_SAFE.
    set(_kde3qt_defs QT_THREAD_SUPPORT)
    if(CMAKE_SYSTEM_NAME MATCHES "BSD")
      list(APPEND _kde3qt_defs _THREAD_SAFE)
    else()
      list(APPEND _kde3qt_defs _REENTRANT)
    endif()

    add_library(KDE3Qt::Qt SHARED IMPORTED)
    set_target_properties(KDE3Qt::Qt PROPERTIES
      IMPORTED_LOCATION "${KDE3Qt_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${KDE3Qt_INCLUDE_DIR}"
      INTERFACE_COMPILE_DEFINITIONS "${_kde3qt_defs}"
      INTERFACE_LINK_LIBRARIES "ZLIB::ZLIB;PNG::PNG;X11::Xext;X11::X11;X11::SM;X11::ICE;Threads::Threads")

    if(KDE3Qt_QUI_LIBRARY)
      add_library(KDE3Qt::QUI SHARED IMPORTED)
      set_target_properties(KDE3Qt::QUI PROPERTIES
        IMPORTED_LOCATION "${KDE3Qt_QUI_LIBRARY}"
        INTERFACE_LINK_LIBRARIES KDE3Qt::Qt)
    endif()

    add_executable(KDE3Qt::moc IMPORTED)
    set_target_properties(KDE3Qt::moc PROPERTIES
      IMPORTED_LOCATION "${KDE3Qt_MOC_EXECUTABLE}")
    add_executable(KDE3Qt::uic IMPORTED)
    set_target_properties(KDE3Qt::uic PROPERTIES
      IMPORTED_LOCATION "${KDE3Qt_UIC_EXECUTABLE}")
  endif()
endif()

mark_as_advanced(KDE3Qt_INCLUDE_DIR KDE3Qt_LIBRARY KDE3Qt_QUI_LIBRARY
  KDE3Qt_MOC_EXECUTABLE KDE3Qt_UIC_EXECUTABLE)
