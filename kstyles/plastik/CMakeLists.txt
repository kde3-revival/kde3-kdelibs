kde3_add_module(plastik
  DESTINATION ${KDE3_STYLE_INSTALL_DIR}
  SOURCES
    plastik.cpp
    misc.cpp
  MOC
    plastik.h
  LINK
    kdefx
)
target_compile_definitions(plastik PRIVATE QT_PLUGIN)

install(FILES
  plastik.themerc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kstyle/themes)

add_subdirectory(config)
