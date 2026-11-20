kde3_add_module(highcontrast
  DESTINATION ${KDE3_STYLE_INSTALL_DIR}
  SOURCES
    highcontrast.cpp
  MOC
    highcontrast.h
  LINK
    kdefx
)
target_compile_definitions(highcontrast PRIVATE QT_PLUGIN)

install(FILES
  highcontrast.themerc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kstyle/themes)

add_subdirectory(config)
