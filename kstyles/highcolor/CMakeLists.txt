kde3_add_module(highcolor
  DESTINATION ${KDE3_STYLE_INSTALL_DIR}
  SOURCES
    highcolor.cpp
  MOC
    highcolor.h
  LINK
    kdefx
)
target_compile_definitions(highcolor PRIVATE QT_PLUGIN)
