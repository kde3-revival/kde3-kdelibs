kde3_add_executable(genembed
  NO_INSTALL
  SOURCES
    genembed.cpp
  LINK
    kdefx
)

# pixmaps.keramik (stdout) and keramikrc.h (written into the working
# directory) from the PNGs in pics/
file(GLOB _pics CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pics/*.png")
list(SORT _pics)
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pixmaps.keramik"
         "${CMAKE_CURRENT_BINARY_DIR}/keramikrc.h"
  COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/pixmaps.keramik"
          "-DCOMMAND=$<TARGET_FILE:genembed>;${_pics}"
          -P "${KDE3_MACROS_DIR}/kde3_run_to_file.cmake"
  DEPENDS genembed ${_pics} "${KDE3_MACROS_DIR}/kde3_run_to_file.cmake"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Generating pixmaps.keramik and keramikrc.h"
  VERBATIM)

kde3_add_module(keramik
  DESTINATION ${KDE3_STYLE_INSTALL_DIR}
  SOURCES
    keramik.cpp
    pixmaploader.cpp
    gradients.cpp
    colorutil.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/pixmaps.keramik
    ${CMAKE_CURRENT_BINARY_DIR}/keramikrc.h
  MOC
    keramik.h
  LINK
    kdefx
)

set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/pixmaps.keramik"
  PROPERTIES HEADER_FILE_ONLY TRUE)
# AM_CPPFLAGS = -DQT_PLUGIN; KDE_CXXFLAGS = -UQT_NO_ASCII_CAST ("qembed's
# output needs that")
target_compile_definitions(keramik PRIVATE QT_PLUGIN)
target_compile_options(keramik PRIVATE -UQT_NO_ASCII_CAST)
