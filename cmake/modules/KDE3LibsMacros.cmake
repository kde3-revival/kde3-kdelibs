#[=======================================================================[.rst:
KDE3LibsMacros
--------------

(Not KDE3Macros: CMake ships a KDE3Macros.cmake of its own, loaded by
its FindKDE3 module, with functions of the same names.  Using both in
one project is an error.)

Functions that replace the KDE 3 automake extensions (admin/am_edit):

``kde3_add_library(<name> SHARED|STATIC|OBJECT [VERSION_INFO c:r:a] ...)``
  A library ``lib<name>``.  SHARED libraries get the SONAME of libtool's
  ``-version-info c:r:a`` (``lib<name>.so.<c-a>.<a>.<r>``), are installed
  into KDE3_LIB_INSTALL_DIR and, in kdelibs, exported as
  ``KDE3::<name>`` with the alias ``KDE3::<name>`` in the build as well
  (other modules pass their own EXPORT set).  OBJECT libraries replace
  libtool convenience libraries (noinst_LTLIBRARIES): link them into the
  library that used them in LIBADD.
``kde3_add_module(<name> ...)``
  A plugin loaded with KLibLoader (``kde_module_LTLIBRARIES``,
  ``-module -avoid-version``): ``<name>.so`` without ``lib`` prefix and
  version, installed into KDE3_MODULE_INSTALL_DIR (or DESTINATION).
``kde3_add_kdeinit_executable(<name> ...)``
  ``kdeinit_LTLIBRARIES = <name>.la``: the code goes into
  ``libkdeinit_<name>.so`` (target ``kdeinit_<name>``), plus an
  executable ``<name>`` calling its kdemain() and a module
  ``<name>.so`` (target ``<name>_module``) exporting kdeinitmain() for
  kdeinit.
``kde3_add_executable(<name> ...)``
  A program, installed into KDE3_BIN_INSTALL_DIR unless NO_INSTALL.

Common arguments:

``SOURCES``
  Sources as in Makefile.am: besides C/C++ files, ``foo.skel`` and
  ``foo.stub`` (DCOP skeleton/stub from ``foo.h``, which may be given
  with a directory: ``../kio/kio/observer.stub``), ``foo.ui`` (uic with
  KDE's post-processing) and ``foo.kcfgc`` (kconfig_compiler).
``MOC <header>...``
  Headers with Q_OBJECT whose ``<base>.moc`` is #included by a source
  (METASOURCES = AUTO).  Generated into the current binary directory.
``MOC_SOURCES <header>...``
  Headers with Q_OBJECT whose moc output no source includes (am_edit
  then compiled ``<base>.moc.cpp`` separately).
``LINK <items>...``
  LIBADD/LDADD: linked PUBLIC for libraries (libtool passed
  dependency_libs on to users), PRIVATE for modules and executables.
``LINK_PRIVATE <items>...``
  Libraries only the target itself needs.
``EXCEPTIONS``
  Compile with ``-fexceptions`` (KDE_CXXFLAGS = $(USE_EXCEPTIONS)).
``NO_INSTALL``, ``DESTINATION <dir>``, ``OUTPUT_NAME <name>``
``EXPORT <set>``
  Export set for installed libraries and programs (default: none; in
  kdelibs, where ``KDE3_BUILDING_KDELIBS`` is set, ``KDE3LibsTargets``
  for libraries).

Other functions:

``kde3_install_icons([AUTO|<appname>...] [DESTINATION <dir>] [SOURCE_DIR <dir>] [EXCLUDE <file>...])``
  KDE_ICON: install ``<size-or-theme>-<type>-<name>.<ext>`` files with
  the am_edit naming rules (``cr22-action-foo.png`` ->
  ``crystalsvg/22x22/actions/foo.png``).  A file whose name does not
  follow them is skipped with a warning (am_edit printed "unknown icon
  type"); EXCLUDE skips files silently.
``kde3_create_handbook(<appname> [LANG <lang>] [FILES ...])``
  KDE_DOCS: index.cache.bz2 via meinproc and installation into
  KDE3_HTML_INSTALL_DIR/<lang>/<appname> with the "common" symlink.
#]=======================================================================]

include_guard(GLOBAL)

# CMake's Modules/KDE3Macros.cmake (included by Modules/FindKDE3.cmake)
# defines kde3_add_executable(), kde3_add_kdeinit_executable(),
# kde3_install_icons() and kde3_create_handbook() with other signatures,
# and FindKDE3 sets KDE3_*_EXECUTABLE cache variables these functions
# use.  Whichever is loaded last silently wins, so refuse the mix: now,
# and at the end of the top-level directory in case it comes later.
function(_kde3libs_check_clash)
  if(COMMAND kde3_automoc OR COMMAND kde3_add_dcop_skels
     OR DEFINED CACHE{KDECONFIG_EXECUTABLE})
    message(FATAL_ERROR "CMake's own FindKDE3/KDE3Macros module has been "
      "loaded in this project (find_package(KDE3) or include(KDE3Macros)).  "
      "It cannot be used together with kdelibs' KDE3Libs package: use "
      "find_package(KDE3Libs) only.")
  endif()
endfunction()
_kde3libs_check_clash()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
  CALL _kde3libs_check_clash)

set(KDE3_MACROS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Tools: inside kdelibs these are the build targets, elsewhere the
# imported KDE3:: executables.  Each may be overridden before inclusion.
foreach(_tool dcopidl dcopidl2cpp kconfig_compiler meinproc)
  string(TOUPPER "${_tool}" _TOOL)
  if(NOT DEFINED KDE3_${_TOOL}_EXECUTABLE)
    if(TARGET KDE3::${_tool})
      set(KDE3_${_TOOL}_EXECUTABLE KDE3::${_tool})
    else()
      set(KDE3_${_TOOL}_EXECUTABLE ${_tool})
    endif()
  endif()
endforeach()

if(NOT DEFINED KDE3_KDECORE_TARGET)
  if(TARGET KDE3::kdecore)
    set(KDE3_KDECORE_TARGET KDE3::kdecore)
  else()
    set(KDE3_KDECORE_TARGET kdecore)
  endif()
endif()

if(NOT DEFINED KDE3_UIC_PLUGIN_DIR)
  # uic -L: where uic finds the KDE designer plugins (kdewidgets) of an
  # installed kdelibs; kdelibs itself does not use it.
  set(KDE3_UIC_PLUGIN_DIR "")
endif()

# KDE3_TOOL_LAUNCHER: prefix for running kdelibs' tools (dcopidl,
# dcopidl2cpp, kconfig_compiler, meinproc, and uic loading the kdewidgets
# plugin).  Inside kdelibs they are build targets and need nothing.
# Elsewhere they are the installed programs of the kdelibs that was
# found, possibly in a DESTDIR staging tree, and kde3-qt possibly not in
# KDE3_QT_RPATH (a development tree): run them with the libraries and
# data of the kdelibs and kde3-qt that were found (KDEDIRS: meinproc
# reads its stylesheets and DTDs through KStandardDirs).  Modules may
# use it for their own calls of the tools (KDE3::makekdewidgets, ...).
if(NOT DEFINED KDE3_TOOL_LAUNCHER)
  set(KDE3_TOOL_LAUNCHER "")
  if(NOT KDE3_BUILDING_KDELIBS AND UNIX AND NOT APPLE AND KDE3_PREFIX)
    set(_kde3_ld "${KDE3_LIB_DIR}")
    if(KDE3Qt_LIBRARY_DIR)
      string(APPEND _kde3_ld ":${KDE3Qt_LIBRARY_DIR}")
    endif()
    set(KDE3_TOOL_LAUNCHER "${CMAKE_COMMAND}" -E env
      "LD_LIBRARY_PATH=${_kde3_ld}" "KDEDIRS=${KDE3_PREFIX}")
  endif()
endif()

# ---------------------------------------------------------------------
# RUNPATH of the installed libraries, modules and programs
#
# Each gets (see _kde3_set_install_rpath):
#  1. $ORIGIN/<path to KDE3_LIB_INSTALL_DIR>, so that an installation
#     works wherever it is copied to (a DESTDIR staging tree, a
#     relocated package), as kde3-qt does it;
#  2. the absolute KDE library directory (of the module, and of kdelibs
#     if that is elsewhere): setuid/setgid programs (kdesud of kdebase,
#     for example) run in secure mode, where ld.so ignores $ORIGIN;
#  3. KDE3_QT_RPATH, the library directory of the installed kde3-qt.
# Directories the linker searches anyway are left out.

if(NOT DEFINED KDE3_QT_RPATH)
  # Where kde3-qt is installed, not the tree kdelibs is built against
  # (which may be a development tree): pkg-config kde3-qt, else the
  # default prefix of kde3-qt.
  set(_kde3_qt_rpath /opt/kde3-qt/lib)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(_KDE3QT_RPATH_PC QUIET kde3-qt)
    if(_KDE3QT_RPATH_PC_FOUND)
      pkg_get_variable(_kde3_qt_rpath kde3-qt libdir)
    endif()
  endif()
  set(KDE3_QT_RPATH "${_kde3_qt_rpath}" CACHE PATH
    "Library directory of the installed kde3-qt, put into the RUNPATH of installed KDE binaries")
endif()

function(_kde3_set_install_rpath target destination)
  if(IS_ABSOLUTE "${destination}")
    set(_full "${destination}")
  else()
    set(_full "${CMAKE_INSTALL_PREFIX}/${destination}")
  endif()
  file(RELATIVE_PATH _rel "${_full}" "${KDE3_FULL_LIB_INSTALL_DIR}")
  string(REGEX REPLACE "/+$" "" _rel "${_rel}")
  if(_rel STREQUAL "")
    set(_rpath "$ORIGIN")
  else()
    set(_rpath "$ORIGIN/${_rel}")
  endif()
  # KDE3_LIBS_LIB_INSTALL_DIR: kdelibs' own, when building other modules
  foreach(_dir "${KDE3_FULL_LIB_INSTALL_DIR}" "${KDE3_LIBS_LIB_INSTALL_DIR}" "${KDE3_QT_RPATH}")
    if(_dir AND NOT _dir IN_LIST _rpath
       AND NOT _dir IN_LIST CMAKE_C_IMPLICIT_LINK_DIRECTORIES
       AND NOT _dir IN_LIST CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
      list(APPEND _rpath "${_dir}")
    endif()
  endforeach()
  set_target_properties(${target} PROPERTIES INSTALL_RPATH "${_rpath}")
endfunction()

# ---------------------------------------------------------------------
# moc

function(_kde3_moc_command header output)
  get_filename_component(_hdr "${header}" ABSOLUTE)
  add_custom_command(OUTPUT "${output}"
    COMMAND KDE3Qt::moc "${_hdr}" -o "${output}"
    DEPENDS "${_hdr}" "$<TARGET_FILE:KDE3Qt::moc>"
    COMMENT "Generating ${output}"
    VERBATIM)
endfunction()

function(kde3_moc target)
  cmake_parse_arguments(PARSE_ARGV 1 M "" "" "INCLUDED;SOURCES")
  foreach(_h IN LISTS M_INCLUDED)
    get_filename_component(_base "${_h}" NAME_WE)
    set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_base}.moc")
    _kde3_moc_command("${_h}" "${_out}")
    target_sources(${target} PRIVATE "${_out}")
    set_source_files_properties("${_out}" PROPERTIES HEADER_FILE_ONLY TRUE)
  endforeach()
  foreach(_h IN LISTS M_SOURCES)
    get_filename_component(_base "${_h}" NAME_WE)
    set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_base}.moc.cpp")
    _kde3_moc_command("${_h}" "${_out}")
    target_sources(${target} PRIVATE "${_out}")
  endforeach()
endfunction()

# ---------------------------------------------------------------------
# DCOP: foo.skel / foo.stub from foo.h (am_edit tag_IDLFILES)

function(_kde3_kidl header kidl)
  get_property(_done DIRECTORY PROPERTY _KDE3_KIDL_${kidl})
  if(_done)
    return()
  endif()
  set_property(DIRECTORY PROPERTY _KDE3_KIDL_${kidl} TRUE)
  add_custom_command(OUTPUT "${kidl}"
    COMMAND ${KDE3_TOOL_LAUNCHER} ${CMAKE_COMMAND}
      "-DDCOPIDL=$<TARGET_FILE:${KDE3_DCOPIDL_EXECUTABLE}>"
      "-DINPUT=${header}" "-DOUTPUT=${kidl}"
      -P "${KDE3_MACROS_DIR}/kde3_dcopidl.cmake"
    DEPENDS "${header}" "$<TARGET_FILE:${KDE3_DCOPIDL_EXECUTABLE}>"
            "${KDE3_MACROS_DIR}/kde3_dcopidl.cmake"
    COMMENT "Generating ${kidl}"
    VERBATIM)
endfunction()

function(_kde3_dcop_source target src kind)
  get_filename_component(_abs "${src}" ABSOLUTE)
  get_filename_component(_dir "${_abs}" DIRECTORY)
  get_filename_component(_base "${_abs}" NAME_WE)
  set(_header "${_dir}/${_base}.h")
  set(_bin "${CMAKE_CURRENT_BINARY_DIR}")
  _kde3_kidl("${_header}" "${_bin}/${_base}.kidl")

  if(kind STREQUAL "skel")
    set(_outputs "${_bin}/${_base}_skel.cpp")
    set(_flags --no-signals --no-stub)
  else()
    set(_outputs "${_bin}/${_base}_stub.h" "${_bin}/${_base}_stub.cpp")
    set(_flags --no-signals --no-skel)
  endif()
  get_property(_done DIRECTORY PROPERTY _KDE3_DCOP_${_base}_${kind})
  if(NOT _done)
    set_property(DIRECTORY PROPERTY _KDE3_DCOP_${_base}_${kind} TRUE)
    add_custom_command(OUTPUT ${_outputs}
      COMMAND ${KDE3_TOOL_LAUNCHER}
              "$<TARGET_FILE:${KDE3_DCOPIDL2CPP_EXECUTABLE}>" --c++-suffix cpp ${_flags}
              "${_base}.kidl"
      DEPENDS "${_bin}/${_base}.kidl"
              "$<TARGET_FILE:${KDE3_DCOPIDL2CPP_EXECUTABLE}>"
      WORKING_DIRECTORY "${_bin}"
      COMMENT "Generating DCOP ${kind} for ${_base}.h"
      VERBATIM)
  endif()
  target_sources(${target} PRIVATE ${_outputs})
endfunction()

# ---------------------------------------------------------------------
# uic (am_edit tag_UIFILES, not "qtonly")

function(_kde3_ui_source target src)
  get_filename_component(_ui "${src}" ABSOLUTE)
  get_filename_component(_base "${_ui}" NAME_WE)
  set(_bin "${CMAKE_CURRENT_BINARY_DIR}")
  set(_h "${_bin}/${_base}.h")
  set(_cpp "${_bin}/${_base}.cpp")
  set(_moc "${_bin}/${_base}.moc")
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" _mangled "${_base}")
  get_property(_done DIRECTORY PROPERTY _KDE3_UI_${_base})
  if(NOT _done)
    set_property(DIRECTORY PROPERTY _KDE3_UI_${_base} TRUE)
    set(_script_args
      "-DUIC=$<TARGET_FILE:KDE3Qt::uic>" "-DUI_FILE=${_ui}"
      "-DPLUGIN_DIR=${KDE3_UIC_PLUGIN_DIR}")
    add_custom_command(OUTPUT "${_h}"
      COMMAND ${KDE3_TOOL_LAUNCHER} ${CMAKE_COMMAND} ${_script_args} "-DHEADER=${_h}"
              -P "${KDE3_MACROS_DIR}/kde3_uic.cmake"
      DEPENDS "${_ui}" "$<TARGET_FILE:KDE3Qt::uic>"
              "${KDE3_MACROS_DIR}/kde3_uic.cmake"
      COMMENT "Generating ${_base}.h from ${_base}.ui"
      VERBATIM)
    add_custom_command(OUTPUT "${_cpp}"
      COMMAND ${KDE3_TOOL_LAUNCHER} ${CMAKE_COMMAND} ${_script_args} "-DHEADER=${_h}"
              "-DSOURCE=${_cpp}" "-DMANGLED=${_mangled}"
              -P "${KDE3_MACROS_DIR}/kde3_uic.cmake"
      DEPENDS "${_ui}" "${_h}" "$<TARGET_FILE:KDE3Qt::uic>"
              "${KDE3_MACROS_DIR}/kde3_uic.cmake"
      COMMENT "Generating ${_base}.cpp from ${_base}.ui"
      VERBATIM)
    _kde3_moc_command("${_h}" "${_moc}")
    set_source_files_properties("${_moc}" PROPERTIES HEADER_FILE_ONLY TRUE)
  endif()
  target_sources(${target} PRIVATE "${_h}" "${_cpp}" "${_moc}")
endfunction()

# ---------------------------------------------------------------------
# kconfig_compiler (am_edit tag_KCFGFILES)

function(_kde3_kcfgc_source target src)
  get_filename_component(_kcfgc "${src}" ABSOLUTE)
  get_filename_component(_dir "${_kcfgc}" DIRECTORY)
  get_filename_component(_base "${_kcfgc}" NAME_WE)
  file(STRINGS "${_kcfgc}" _file REGEX "^File=")
  if(NOT _file)
    message(FATAL_ERROR "${src}: no File= entry")
  endif()
  string(REGEX REPLACE "^File=[ \t]*([^ \t]*).*" "\\1" _file "${_file}")
  set(_kcfg "${_dir}/${_file}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_kcfgc}")
  set(_bin "${CMAKE_CURRENT_BINARY_DIR}")
  get_property(_done DIRECTORY PROPERTY _KDE3_KCFGC_${_base})
  if(NOT _done)
    set_property(DIRECTORY PROPERTY _KDE3_KCFGC_${_base} TRUE)
    add_custom_command(OUTPUT "${_bin}/${_base}.h" "${_bin}/${_base}.cpp"
      COMMAND ${KDE3_TOOL_LAUNCHER}
              "$<TARGET_FILE:${KDE3_KCONFIG_COMPILER_EXECUTABLE}>" -d "${_bin}/"
              "${_kcfg}" "${_kcfgc}"
      DEPENDS "${_kcfg}" "${_kcfgc}"
              "$<TARGET_FILE:${KDE3_KCONFIG_COMPILER_EXECUTABLE}>"
      COMMENT "Generating ${_base}.h and ${_base}.cpp from ${_file}"
      VERBATIM)
  endif()
  target_sources(${target} PRIVATE "${_bin}/${_base}.h" "${_bin}/${_base}.cpp")
endfunction()

# ---------------------------------------------------------------------

function(_kde3_add_sources target)
  set(_plain)
  foreach(_src IN LISTS ARGN)
    if(_src MATCHES "\\.skel$")
      _kde3_dcop_source(${target} "${_src}" skel)
    elseif(_src MATCHES "\\.stub$")
      _kde3_dcop_source(${target} "${_src}" stub)
    elseif(_src MATCHES "\\.ui$")
      _kde3_ui_source(${target} "${_src}")
    elseif(_src MATCHES "\\.kcfgc$")
      _kde3_kcfgc_source(${target} "${_src}")
    else()
      list(APPEND _plain "${_src}")
    endif()
  endforeach()
  if(_plain)
    target_sources(${target} PRIVATE ${_plain})
  endif()
  # automake's DEFAULT_INCLUDES (-I. -I$(srcdir)), searched before the
  # INCLUDES: generated files and the .moc files are found here.
  target_include_directories(${target} BEFORE PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

set(_KDE3_COMMON_OPTIONS EXCEPTIONS NO_INSTALL)
set(_KDE3_COMMON_ONE DESTINATION OUTPUT_NAME EXPORT)
set(_KDE3_COMMON_MULTI SOURCES MOC MOC_SOURCES LINK LINK_PRIVATE)

function(_kde3_setup_target target link_mode)
  # ARG_* of the caller are visible here.
  _kde3_add_sources(${target} ${ARG_SOURCES})
  kde3_moc(${target} INCLUDED ${ARG_MOC} SOURCES ${ARG_MOC_SOURCES})
  if(ARG_LINK)
    target_link_libraries(${target} ${link_mode} ${ARG_LINK})
  endif()
  if(ARG_LINK_PRIVATE)
    target_link_libraries(${target} PRIVATE ${ARG_LINK_PRIVATE})
  endif()
  if(ARG_EXCEPTIONS)
    target_compile_options(${target} PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:-fexceptions>")
  endif()
  if(ARG_OUTPUT_NAME)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
  endif()
endfunction()

function(kde3_add_library name type)
  cmake_parse_arguments(PARSE_ARGV 2 ARG "${_KDE3_COMMON_OPTIONS}"
    "${_KDE3_COMMON_ONE};VERSION_INFO" "${_KDE3_COMMON_MULTI}")
  if(NOT type MATCHES "^(SHARED|STATIC|OBJECT)$")
    message(FATAL_ERROR "kde3_add_library(${name}): type must be SHARED, STATIC or OBJECT")
  endif()
  add_library(${name} ${type})
  if(type STREQUAL "SHARED")
    _kde3_setup_target(${name} PUBLIC)
    if(ARG_VERSION_INFO)
      # libtool: current[:revision[:age]], missing parts are 0;
      # lib<name>.so.<current-age>.<age>.<revision>
      if(NOT ARG_VERSION_INFO MATCHES "^([0-9]+)(:([0-9]+))?(:([0-9]+))?$")
        message(FATAL_ERROR "kde3_add_library(${name}): VERSION_INFO must be current[:revision[:age]]")
      endif()
      set(_cur "${CMAKE_MATCH_1}")
      set(_rev "${CMAKE_MATCH_3}")
      set(_age "${CMAKE_MATCH_5}")
      if(_rev STREQUAL "")
        set(_rev 0)
      endif()
      if(_age STREQUAL "")
        set(_age 0)
      endif()
      math(EXPR _major "${_cur} - ${_age}")
      set_target_properties(${name} PROPERTIES
        VERSION "${_major}.${_age}.${_rev}"
        SOVERSION "${_major}")
    endif()
    target_include_directories(${name} INTERFACE
      "$<INSTALL_INTERFACE:${KDE3_INCLUDE_INSTALL_DIR}>")
    # The KDE3:: names and the KDE3LibsTargets export set belong to
    # kdelibs; other modules export only into a set they name.
    if(KDE3_BUILDING_KDELIBS)
      add_library(KDE3::${name} ALIAS ${name})
      if(NOT ARG_EXPORT)
        set(ARG_EXPORT KDE3LibsTargets)
      endif()
    endif()
    if(NOT ARG_NO_INSTALL)
      if(NOT ARG_DESTINATION)
        set(ARG_DESTINATION "${KDE3_LIB_INSTALL_DIR}")
      endif()
      if(ARG_EXPORT)
        install(TARGETS ${name} EXPORT ${ARG_EXPORT}
          LIBRARY DESTINATION "${ARG_DESTINATION}")
      else()
        install(TARGETS ${name} LIBRARY DESTINATION "${ARG_DESTINATION}")
      endif()
      _kde3_set_install_rpath(${name} "${ARG_DESTINATION}")
    endif()
  else()
    # libtool convenience library: position independent, linked into
    # a shared library or module.
    _kde3_setup_target(${name} PUBLIC)
    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
endfunction()

function(kde3_add_module name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "${_KDE3_COMMON_OPTIONS}"
    "${_KDE3_COMMON_ONE}" "${_KDE3_COMMON_MULTI}")
  add_library(${name} MODULE)
  _kde3_setup_target(${name} PRIVATE)
  set_target_properties(${name} PROPERTIES PREFIX "")
  if(NOT ARG_NO_INSTALL)
    if(NOT ARG_DESTINATION)
      set(ARG_DESTINATION "${KDE3_MODULE_INSTALL_DIR}")
    endif()
    install(TARGETS ${name} LIBRARY DESTINATION "${ARG_DESTINATION}")
    _kde3_set_install_rpath(${name} "${ARG_DESTINATION}")
  endif()
endfunction()

function(kde3_add_executable name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "${_KDE3_COMMON_OPTIONS}"
    "${_KDE3_COMMON_ONE}" "${_KDE3_COMMON_MULTI}")
  add_executable(${name})
  _kde3_setup_target(${name} PRIVATE)
  if(NOT ARG_NO_INSTALL)
    if(NOT ARG_DESTINATION)
      set(ARG_DESTINATION "${KDE3_BIN_INSTALL_DIR}")
    endif()
    if(ARG_EXPORT)
      install(TARGETS ${name} EXPORT ${ARG_EXPORT} RUNTIME DESTINATION "${ARG_DESTINATION}")
    else()
      install(TARGETS ${name} RUNTIME DESTINATION "${ARG_DESTINATION}")
    endif()
    _kde3_set_install_rpath(${name} "${ARG_DESTINATION}")
  endif()
endfunction()

# am_edit tag_KDEINIT: libkdeinit_<name>.so with the code, the program
# <name> and the kdeinit module <name>.so.
function(kde3_add_kdeinit_executable name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "${_KDE3_COMMON_OPTIONS}"
    "${_KDE3_COMMON_ONE}" "${_KDE3_COMMON_MULTI}")
  set(_lib kdeinit_${name})
  add_library(${_lib} SHARED)
  _kde3_setup_target(${_lib} PRIVATE)

  set(_bin "${CMAKE_CURRENT_BINARY_DIR}")
  file(CONFIGURE OUTPUT "${_bin}/${name}_kdeinit_main.cpp" CONTENT
"extern \"C\" int kdemain(int argc, char* argv[]);
int main(int argc, char* argv[]) { return kdemain(argc,argv); }
")
  file(CONFIGURE OUTPUT "${_bin}/${name}_kdeinit_module.cpp" CONTENT
"#include <kdemacros.h>
extern \"C\" int kdemain(int argc, char* argv[]);
extern \"C\" KDE_EXPORT int kdeinitmain(int argc, char* argv[]) { return kdemain(argc,argv); }
")
  add_executable(${name} "${_bin}/${name}_kdeinit_main.cpp")
  target_link_libraries(${name} PRIVATE ${_lib})
  add_library(${name}_module MODULE "${_bin}/${name}_kdeinit_module.cpp")
  target_link_libraries(${name}_module PRIVATE ${_lib})
  # only for <kdemacros.h>
  target_include_directories(${name}_module PRIVATE
    "$<TARGET_PROPERTY:${KDE3_KDECORE_TARGET},INTERFACE_INCLUDE_DIRECTORIES>")
  set_target_properties(${name}_module PROPERTIES PREFIX "" OUTPUT_NAME ${name})

  if(NOT ARG_NO_INSTALL)
    install(TARGETS ${_lib} LIBRARY DESTINATION "${KDE3_LIB_INSTALL_DIR}")
    install(TARGETS ${name} RUNTIME DESTINATION "${KDE3_BIN_INSTALL_DIR}")
    install(TARGETS ${name}_module LIBRARY DESTINATION "${KDE3_MODULE_INSTALL_DIR}")
    _kde3_set_install_rpath(${_lib} "${KDE3_LIB_INSTALL_DIR}")
    _kde3_set_install_rpath(${name} "${KDE3_BIN_INSTALL_DIR}")
    _kde3_set_install_rpath(${name}_module "${KDE3_MODULE_INSTALL_DIR}")
  endif()
endfunction()

# ---------------------------------------------------------------------
# KDE_ICON (am_edit tag_ICON)

function(kde3_install_icons)
  cmake_parse_arguments(PARSE_ARGV 0 ARG "" "DESTINATION;SOURCE_DIR" "EXCLUDE")
  if(NOT ARG_DESTINATION)
    set(ARG_DESTINATION "${KDE3_ICON_INSTALL_DIR}")
  endif()
  if(NOT ARG_SOURCE_DIR)
    set(ARG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  set(_names ${ARG_UNPARSED_ARGUMENTS})
  if(NOT _names)
    set(_names AUTO)
  endif()

  file(GLOB _all CONFIGURE_DEPENDS RELATIVE "${ARG_SOURCE_DIR}"
    "${ARG_SOURCE_DIR}/*.png" "${ARG_SOURCE_DIR}/*.xpm"
    "${ARG_SOURCE_DIR}/*.mng" "${ARG_SOURCE_DIR}/*.svg"
    "${ARG_SOURCE_DIR}/*.svgz")
  if(ARG_EXCLUDE)
    list(REMOVE_ITEM _all ${ARG_EXCLUDE})
  endif()
  set(_files)
  if(_names STREQUAL "AUTO")
    set(_files ${_all})
  else()
    foreach(_f IN LISTS _all)
      foreach(_n IN LISTS _names)
        string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1" _qn "${_n}")
        if(_f MATCHES "-${_qn}\\.(png|xpm|mng|svg)")
          list(APPEND _files "${_f}")
          break()
        endif()
      endforeach()
    endforeach()
  endif()

  set(_types action=actions app=apps device=devices filesys=filesystems
             mime=mimetypes)
  set(_dirs los=locolor/16x16 lom=locolor/32x32 him=hicolor/32x32
    hil=hicolor/48x48 lo16=locolor/16x16 lo22=locolor/22x22
    lo32=locolor/32x32 hi16=hicolor/16x16 hi22=hicolor/22x22
    hi32=hicolor/32x32 hi48=hicolor/48x48 hi64=hicolor/64x64
    hi128=hicolor/128x128 hisc=hicolor/scalable cr16=crystalsvg/16x16
    cr22=crystalsvg/22x22 cr32=crystalsvg/32x32 cr48=crystalsvg/48x48
    cr64=crystalsvg/64x64 cr128=crystalsvg/128x128
    crsc=crystalsvg/scalable)

  foreach(_file IN LISTS _files)
    # $prefix = file without extension; $appname = it without the first
    # two "-"-separated fields; $prefix = what is left before them.
    string(REGEX REPLACE "\\.(png|xpm|mng|svg|svgz)$" "" _prefix "${_file}")
    set(_appname "${_prefix}")
    foreach(_i 1 2)
      if(_appname MATCHES "-")
        string(REGEX REPLACE "^[^-]+-" "" _appname "${_appname}")
      endif()
    endforeach()
    string(LENGTH "${_prefix}" _plen)
    string(LENGTH "${_appname}" _alen)
    math(EXPR _keep "${_plen} - ${_alen}")
    string(SUBSTRING "${_prefix}" 0 ${_keep} _prefix)
    string(REGEX REPLACE "-$" "" _prefix "${_prefix}")

    if(_prefix STREQUAL "mini")
      set(_prefix lo16-app)
    elseif(_prefix STREQUAL "lo")
      set(_prefix lo32-app)
    elseif(_prefix STREQUAL "large")
      set(_prefix hi48-app)
    elseif(_prefix MATCHES "^...$")
      string(APPEND _prefix "-app")
    endif()

    string(REGEX REPLACE "^.*-([^-]+)$" "\\1" _type "${_prefix}")
    string(REGEX REPLACE "^(.*)-[^-]+$" "\\1" _size "${_prefix}")

    set(_typedir)
    foreach(_kv IN LISTS _types)
      if(_kv MATCHES "^${_type}=(.*)$")
        set(_typedir "${CMAKE_MATCH_1}")
      endif()
    endforeach()
    if(NOT _typedir)
      message(WARNING "unknown icon type ${_type} (${_file})")
      continue()
    endif()
    set(_sizedir)
    foreach(_kv IN LISTS _dirs)
      if(_kv MATCHES "^${_size}=(.*)$")
        set(_sizedir "${CMAKE_MATCH_1}")
      endif()
    endforeach()
    if(NOT _sizedir)
      message(WARNING "unknown icon prefix ${_size} (${_file})")
      continue()
    endif()

    # $newfile = "$appname.<ext>"
    string(REGEX MATCH "\\.(png|xpm|mng|svgz|svg)$" _ext "${_file}")
    set(_newfile "${_appname}${_ext}")
    set(_dir "${_sizedir}/${_typedir}")
    if(_newfile MATCHES "-[^.]")
      string(REGEX REPLACE "^([^-]+)-.*$" "\\1" _sub "${_newfile}")
      string(APPEND _dir "/${_sub}")
      string(REGEX REPLACE "^[^-]+-" "" _newfile "${_newfile}")
    endif()
    install(FILES "${ARG_SOURCE_DIR}/${_file}"
      DESTINATION "${ARG_DESTINATION}/${_dir}" RENAME "${_newfile}")
  endforeach()
endfunction()

# ---------------------------------------------------------------------
# KDE_DOCS (am_edit tag_DOCFILES)

function(kde3_create_handbook appname)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "LANG" "FILES")
  if(NOT ARG_LANG)
    set(ARG_LANG en)
  endif()
  if(appname STREQUAL "AUTO")
    get_filename_component(appname "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
  endif()
  if(NOT ARG_FILES)
    file(GLOB ARG_FILES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/*")
    list(FILTER ARG_FILES EXCLUDE REGEX
      "^(CVS|Makefile.*|CMakeLists\\.txt|index\\.cache\\.bz2|core|\\..*|.*~|#.*#)$")
    set(_files)
    foreach(_f IN LISTS ARG_FILES)
      if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
        list(APPEND _files "${_f}")
      endif()
    endforeach()
    set(ARG_FILES ${_files})
  endif()
  set(_dest "${KDE3_HTML_INSTALL_DIR}/${ARG_LANG}/${appname}")
  set(_srcfiles)
  foreach(_f IN LISTS ARG_FILES)
    list(APPEND _srcfiles "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
  endforeach()
  install(FILES ${_srcfiles} DESTINATION "${_dest}")

  if("index.docbook" IN_LIST ARG_FILES)
    set(_cache "${CMAKE_CURRENT_BINARY_DIR}/index.cache.bz2")
    if(NOT DEFINED KDE3_MEINPROC_ARGS)
      set(KDE3_MEINPROC_ARGS)
    endif()
    add_custom_command(OUTPUT "${_cache}"
      COMMAND ${KDE3_TOOL_LAUNCHER}
              "$<TARGET_FILE:${KDE3_MEINPROC_EXECUTABLE}>" ${KDE3_MEINPROC_ARGS} --check
              --cache "${_cache}" "${CMAKE_CURRENT_SOURCE_DIR}/index.docbook"
      DEPENDS ${_srcfiles} "$<TARGET_FILE:${KDE3_MEINPROC_EXECUTABLE}>"
              ${KDE3_MEINPROC_DEPENDS}
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      COMMENT "Generating handbook cache for ${appname} (${ARG_LANG})"
      VERBATIM)
    string(MAKE_C_IDENTIFIER "handbook_${ARG_LANG}_${appname}" _t)
    add_custom_target(${_t} ALL DEPENDS "${_cache}")
    install(FILES "${_cache}" DESTINATION "${_dest}")
    # "common" -> $(kde_libs_htmldir)/<lang>/common
    install(CODE "
      file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${_dest}\")
      file(REMOVE \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${_dest}/common\")
      file(CREATE_LINK \"${KDE3_FULL_HTML_INSTALL_DIR}/${ARG_LANG}/common\"
        \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${_dest}/common\" SYMBOLIC)")
  endif()
endfunction()

# ---------------------------------------------------------------------
# kde3_generate_stdout(<output> COMMAND <cmd> <args>... [INPUT <file>]
#                      [DEPENDS ...])
#
# For Makefile.am rules of the form "output: deps; command [< in] > $@".

function(kde3_generate_stdout output)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "INPUT" "COMMAND;DEPENDS")
  get_filename_component(_name "${output}" NAME)
  set(_input)
  if(ARG_INPUT)
    set(_input "-DINPUT=${ARG_INPUT}")
  endif()
  add_custom_command(OUTPUT "${output}"
    COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${output}" "-DCOMMAND=${ARG_COMMAND}"
            ${_input} -P "${KDE3_MACROS_DIR}/kde3_run_to_file.cmake"
    DEPENDS ${ARG_DEPENDS} ${ARG_INPUT}
            "${KDE3_MACROS_DIR}/kde3_run_to_file.cmake"
    COMMENT "Generating ${_name}"
    VERBATIM)
endfunction()

# ---------------------------------------------------------------------
# kde3_install_setuid_root(<file in KDE3_BIN_INSTALL_DIR>)
#
# The "(chown root $(DESTDIR)$(bindir)/x && chmod 4755 ...) || echo"
# install hooks: make an installed program setuid root when the
# installation runs with the permission to do so, otherwise only print
# a note (packagers set the mode themselves).

function(kde3_install_setuid_root name)
  install(CODE "
    set(_f \"\$ENV{DESTDIR}${KDE3_FULL_BIN_INSTALL_DIR}/${name}\")
    execute_process(COMMAND chown 0 \"\${_f}\" RESULT_VARIABLE _r
      OUTPUT_QUIET ERROR_QUIET)
    if(_r EQUAL 0)
      execute_process(COMMAND chmod 4755 \"\${_f}\")
      message(STATUS \"${name} is installed setuid root\")
    else()
      message(STATUS \"Please make ${name} setuid root\")
    endif()")
endfunction()

# kde3_install_symlink(<target> <link>): install a symbolic link <link>
# (path relative to CMAKE_INSTALL_PREFIX, or absolute) pointing to
# <target> ($(LN_S) in install hooks).

function(kde3_install_symlink target link)
  if(NOT IS_ABSOLUTE "${link}")
    set(link "${CMAKE_INSTALL_PREFIX}/${link}")
  endif()
  install(CODE "
    set(_l \"\$ENV{DESTDIR}${link}\")
    get_filename_component(_d \"\${_l}\" DIRECTORY)
    file(MAKE_DIRECTORY \"\${_d}\")
    file(REMOVE \"\${_l}\")
    file(CREATE_LINK \"${target}\" \"\${_l}\" SYMBOLIC)")
endfunction()
