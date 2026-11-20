# INCLUDES
include_directories(
  "${PROJECT_SOURCE_DIR}/kioslave"
  "${PROJECT_SOURCE_DIR}/kioslave/bzip2")

# libkbzipfilter_dummy.la: kioslave/bzip2's filter compiled in
kde3_add_library(kbzipfilter_dummy OBJECT
  SOURCES
    ${PROJECT_SOURCE_DIR}/kioslave/bzip2/kbzip2filter.cpp
  LINK
    kio
    BZip2::BZip2
    LibXslt::LibXslt
    LibXml2::LibXml2
)

foreach(_slave help ghelp)
  set(_main main)
  if(_slave STREQUAL "ghelp")
    set(_main main_ghelp)
  endif()
  kde3_add_module(kio_${_slave}
    SOURCES
      kio_help.cpp
      ${_main}.cpp
      xslt.cpp
    LINK
      kbzipfilter_dummy
      LibXslt::LibXslt
      LibXml2::LibXml2
  )
endforeach()

# meinproc with xslt.cpp compiled again with -DSIMPLE_XSLT (xslt_pure.o)
add_library(xslt_pure OBJECT xslt.cpp)
target_compile_definitions(xslt_pure PRIVATE SIMPLE_XSLT)
target_include_directories(xslt_pure PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(xslt_pure PRIVATE kio LibXslt::LibXslt LibXml2::LibXml2)

kde3_add_executable(meinproc
  EXPORT KDE3Targets
  SOURCES
    meinproc.cpp
  LINK
    xslt_pure
    kbzipfilter_dummy
    LibXslt::LibXslt
    LibXml2::LibXml2
)
add_executable(KDE3::meinproc ALIAS meinproc)

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
if(IS_ABSOLUTE "${KDE3_BIN_INSTALL_DIR}")
  set(bindir "${KDE3_BIN_INSTALL_DIR}")
else()
  set(bindir "\${exec_prefix}/${KDE3_BIN_INSTALL_DIR}")
endif()
configure_file(checkXML.in "${CMAKE_CURRENT_BINARY_DIR}/checkXML" @ONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/checkXML"
  DESTINATION ${KDE3_BIN_INSTALL_DIR})

install(FILES
  help.protocol
  ghelp.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

install(FILES
  kio_help.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)
install(PROGRAMS
  move_kio_help_cache.sh
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

# install-data-local: the DocBook and customization files
install(DIRECTORY customization docbook
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/ksgmltools2
  PATTERN CVS EXCLUDE
  PATTERN .svn EXCLUDE
  REGEX "/\\.[^/]*$" EXCLUDE)
