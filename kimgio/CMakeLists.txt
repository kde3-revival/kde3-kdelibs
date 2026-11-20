# KIMGIO_PLUGIN: -export-symbols-regex 'kimgio_.*_(read|write)'
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/kimgio.map" CONTENT
"{ global: kimgio_*_read; kimgio_*_write; local: *; };
")

# INCLUDES: -Drestrict=
add_compile_definitions(restrict=)

# <source>=<library besides Qt>
set(_plugins eps=kdecore xview= ico= pcx=kdecore tga=kdecore rgb=kdecore
  xcf=kdecore dds=kdecore psd=kdecore hdr=kdecore)
if(HAVE_LIBTIFF)
  # include_TIFF_MODULES
  list(APPEND _plugins tiffr=TIFF::TIFF)
endif()

foreach(_p IN LISTS _plugins)
  string(REGEX MATCH "^([^=]*)=(.*)$" _m "${_p}")
  set(_src "${CMAKE_MATCH_1}")
  set(_lib "${CMAKE_MATCH_2}")
  set(_name ${_src})
  if(_src STREQUAL "tiffr")
    set(_name tiff)
  endif()
  kde3_add_module(kimg_${_name}
    EXCEPTIONS
    SOURCES
      ${_src}.cpp
    LINK
      KDE3Qt::Qt
      ${_lib}
  )
  target_link_options(kimg_${_name} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/kimgio.map")
endforeach()

set(_kimgio png xpm bmp pbm pgm ppm xbm jpeg xv eps ico gif pcx tga rgb xcf
  dds mng psd hdr)
if(HAVE_LIBTIFF)
  list(APPEND _kimgio tiff)
endif()
list(TRANSFORM _kimgio APPEND .kimgio)
install(FILES ${_kimgio} DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
