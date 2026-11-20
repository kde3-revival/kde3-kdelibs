#    This file is part of the KDE libraries
#
#    This library is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Library General Public
#    License as published by the Free Software Foundation; either
#    version 2 of the License, or (at your option) any later version.

cmake_minimum_required(VERSION 3.20...4.4)

project(kdelibs VERSION 3.5.10 LANGUAGES C CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX /opt/kde3 CACHE PATH "Install prefix" FORCE)
endif()

set(KDELIBS_VERSION "${PROJECT_VERSION}")

list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

include(KDE3InstallDirs)
include(KDE3CompilerSettings)

find_package(KDE3Qt REQUIRED)
find_package(X11 REQUIRED)
find_package(Threads REQUIRED)

include(ConfigureChecks.cmake)
configure_file(config.h.cmake "${PROJECT_BINARY_DIR}/config.h")
# automake passes -DHAVE_CONFIG_H (DEFS); kjs, for example, tests it.
add_compile_definitions(HAVE_CONFIG_H)
# configure.in.mid: CXXFLAGS="$CXXFLAGS $KDE_DEFAULT_CXXFLAGS" (C++ only)
add_compile_definitions(
  "$<$<COMPILE_LANGUAGE:CXX>:QT_CLEAN_NAMESPACE;QT_NO_ASCII_CAST;QT_NO_STL;QT_NO_COMPAT;QT_NO_TRANSLATION>")

# Installed libraries and modules find kde3-qt and each other.
set(KDE3_QT_RPATH "${KDE3Qt_LIBRARY_DIR}" CACHE PATH
  "Library directory of kde3-qt to put into the installed RUNPATH")
set(CMAKE_INSTALL_RPATH "${KDE3_FULL_LIB_INSTALL_DIR}")
if(NOT KDE3_QT_RPATH STREQUAL KDE3_FULL_LIB_INSTALL_DIR)
  list(APPEND CMAKE_INSTALL_RPATH "${KDE3_QT_RPATH}")
endif()

# -I$(top_builddir) of every Makefile (config.h) and all_includes of
# configure.in.mid: every directory of kdelibs is compiled with these
# (kdecore uses kio and kdeui headers, for example).  Installed targets
# only carry the include directory of the installation.
include_directories(
  "${PROJECT_BINARY_DIR}"
  "${PROJECT_SOURCE_DIR}/dcop"
  "${PROJECT_SOURCE_DIR}/kdefx"
  "${PROJECT_BINARY_DIR}/kdecore"
  "${PROJECT_SOURCE_DIR}/kdecore"
  "${PROJECT_SOURCE_DIR}/kdecore/network"
  "${PROJECT_SOURCE_DIR}/kdeui"
  "${PROJECT_SOURCE_DIR}/kio"
  "${PROJECT_SOURCE_DIR}/kio/kio"
  "${PROJECT_SOURCE_DIR}/kio/kfile"
  "${PROJECT_SOURCE_DIR}")

# meinproc needs the stylesheets of the source tree while building.
set(KDE3_MEINPROC_ARGS "--srcdir=${PROJECT_SOURCE_DIR}/kdoctools")

include(KDE3Macros)

add_subdirectory(dcop)
add_subdirectory(kdefx)
add_subdirectory(kdecore)
add_subdirectory(kdeui)
add_subdirectory(kdesu)
add_subdirectory(kjs)
add_subdirectory(kwallet)
add_subdirectory(kio)
add_subdirectory(kded)
add_subdirectory(kparts)
add_subdirectory(kinit)
add_subdirectory(kutils)
add_subdirectory(knotify)
add_subdirectory(kdeprint)
add_subdirectory(khtml)
add_subdirectory(kresources)
add_subdirectory(kabc)
add_subdirectory(interfaces)
add_subdirectory(kimgio)
add_subdirectory(kstyles)
add_subdirectory(kconf_update)
add_subdirectory(kcmshell)
add_subdirectory(licenses)
add_subdirectory(libkscreensaver)
add_subdirectory(knewstuff)
add_subdirectory(dnssd)
add_subdirectory(kioslave)
add_subdirectory(kdoctools)
add_subdirectory(doc)
add_subdirectory(mimetypes)
add_subdirectory(pics)
add_subdirectory(kdewidgets)
add_subdirectory(kate)

# ---------------------------------------------------------------------
# CMake package for the other KDE 3 modules

include(CMakePackageConfigHelpers)
get_filename_component(KDE3_QT_PREFIX "${KDE3Qt_INCLUDE_DIR}" DIRECTORY)
configure_package_config_file(cmake/KDE3Config.cmake.in
  "${PROJECT_BINARY_DIR}/KDE3Config.cmake"
  INSTALL_DESTINATION "${KDE3_CMAKE_INSTALL_DIR}"
  PATH_VARS KDE3_INCLUDE_INSTALL_DIR KDE3_LIB_INSTALL_DIR KDE3_MODULE_INSTALL_DIR)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/KDE3ConfigVersion.cmake"
  VERSION "${KDELIBS_VERSION}"
  COMPATIBILITY SameMinorVersion)
install(EXPORT KDE3Targets
  NAMESPACE KDE3::
  DESTINATION "${KDE3_CMAKE_INSTALL_DIR}"
  FILE KDE3Targets.cmake)
install(FILES
  "${PROJECT_BINARY_DIR}/KDE3Config.cmake"
  "${PROJECT_BINARY_DIR}/KDE3ConfigVersion.cmake"
  cmake/modules/FindKDE3Qt.cmake
  cmake/modules/KDE3InstallDirs.cmake
  cmake/modules/KDE3CompilerSettings.cmake
  cmake/modules/KDE3Macros.cmake
  cmake/modules/kde3_dcopidl.cmake
  cmake/modules/kde3_uic.cmake
  cmake/modules/kde3_run_to_file.cmake
  DESTINATION "${KDE3_CMAKE_INSTALL_DIR}")
