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
# kde3_use_dcopidlng(): kalyptus and its modules
set(KDE3_DCOPIDLNG_DIR "${PROJECT_SOURCE_DIR}/dcop/dcopidlng")

set(KDE3_BUILDING_KDELIBS TRUE)
include(KDE3LibsMacros)

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

# potdir = $(kde_includes): the message template of common_texts.cpp
install(FILES kde.pot DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

# ---------------------------------------------------------------------
# CMake package for the other KDE 3 modules

include(CMakePackageConfigHelpers)
get_filename_component(KDE3_QT_PREFIX "${KDE3Qt_INCLUDE_DIR}" DIRECTORY)
# find_package(KDE3Libs): not "KDE3", which CMake's own FindKDE3.cmake
# module (and its KDE3Macros.cmake) would take in module mode.
configure_package_config_file(cmake/KDE3LibsConfig.cmake.in
  "${PROJECT_BINARY_DIR}/KDE3LibsConfig.cmake"
  INSTALL_DESTINATION "${KDE3_CMAKE_INSTALL_DIR}"
  PATH_VARS KDE3_INCLUDE_INSTALL_DIR KDE3_LIB_INSTALL_DIR KDE3_MODULE_INSTALL_DIR
            KDE3_DATA_INSTALL_DIR)
# find_package(KDE3Libs 3) and find_package(KDE3Libs 3.5) both work
write_basic_package_version_file("${PROJECT_BINARY_DIR}/KDE3LibsConfigVersion.cmake"
  VERSION "${KDELIBS_VERSION}"
  COMPATIBILITY SameMajorVersion)
install(EXPORT KDE3LibsTargets
  NAMESPACE KDE3::
  DESTINATION "${KDE3_CMAKE_INSTALL_DIR}"
  FILE KDE3LibsTargets.cmake)
install(FILES
  "${PROJECT_BINARY_DIR}/KDE3LibsConfig.cmake"
  "${PROJECT_BINARY_DIR}/KDE3LibsConfigVersion.cmake"
  cmake/modules/FindKDE3Qt.cmake
  cmake/modules/KDE3InstallDirs.cmake
  cmake/modules/KDE3CompilerSettings.cmake
  cmake/modules/KDE3LibsMacros.cmake
  cmake/modules/kde3_dcopidl.cmake
  cmake/modules/kde3_uic.cmake
  cmake/modules/kde3_run_to_file.cmake
  DESTINATION "${KDE3_CMAKE_INSTALL_DIR}")

# what configure printed at its end: the packages found or missing and
# the KDE3_WITH_*/KDE3_ENABLE_* options
include(FeatureSummary)
# option: what it turned on (enabled and found)
foreach(_f
    WITH_ACL:USE_POSIX_ACL WITH_DNSSD:HAVE_DNSSD WITH_FAM:HAVE_FAM
    WITH_GSSAPI:HAVE_LIBGSSAPI WITH_LIBIDN:HAVE_LIBIDN WITH_OPENSSL:HAVE_SSL
    WITH_PCRE:HAVE_PCREPOSIX WITH_PULSEAUDIO:HAVE_PULSEAUDIO
    WITH_TIFF:HAVE_LIBTIFF WITH_UTEMPTER:HAVE_UTEMPTER
    WITH_LIBTHAI:HAVE_LIBTHAI WITH_LUA:HAVE_LUA ENABLE_INOTIFY:HAVE_INOTIFY
    ENABLE_DNOTIFY:HAVE_DNOTIFY ENABLE_MITSHM:HAVE_MITSHM
    ENABLE_SENDFILE:HAVE_SENDFILE ENABLE_WARNINGS:KDE3_ENABLE_WARNINGS)
  string(REPLACE ":" ";" _f "${_f}")
  list(GET _f 0 _opt)
  list(GET _f 1 _have)
  get_property(_doc CACHE KDE3_${_opt} PROPERTY HELPSTRING)
  add_feature_info(KDE3_${_opt} ${_have} "${_doc}")
endforeach()
feature_summary(WHAT PACKAGES_FOUND PACKAGES_NOT_FOUND ENABLED_FEATURES DISABLED_FEATURES)
