kde3_add_module(ktexteditor_isearch
  SOURCES
    ISearchPlugin.cpp
  MOC
    ISearchPlugin.h
  LINK
    ktexteditor
)

install(FILES
  ktexteditor_isearchui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/ktexteditor_isearch)

install(FILES
  ktexteditor_isearch.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(ktexteditor_isearch PRIVATE "${PROJECT_SOURCE_DIR}/interfaces")
