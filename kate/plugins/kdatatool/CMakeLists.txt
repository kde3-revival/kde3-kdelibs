kde3_add_module(ktexteditor_kdatatool
  SOURCES
    kate_kdatatool.cpp
  MOC
    kate_kdatatool.h
  LINK
    ktexteditor
)

install(FILES
  ktexteditor_kdatatoolui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/ktexteditor_kdatatool)

install(FILES
  ktexteditor_kdatatool.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(ktexteditor_kdatatool PRIVATE "${PROJECT_SOURCE_DIR}/interfaces")
