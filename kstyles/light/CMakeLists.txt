kde3_add_module(light
  DESTINATION ${KDE3_STYLE_INSTALL_DIR}
  SOURCES
    light.cpp
    lightstyle-v2.cpp
    lightstyle-v3.cpp
  MOC
    lightstyle-v2.h
    lightstyle-v3.h
  LINK
    KDE3Qt::Qt
    kdefx
)
target_compile_definitions(light PRIVATE QT_PLUGIN)
