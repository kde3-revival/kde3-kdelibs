kde3_add_module(ktexteditor_insertfile
  SOURCES
    insertfileplugin.cpp
  MOC
    insertfileplugin.h
  LINK
    ktexteditor
)

install(FILES
  ktexteditor_insertfileui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/ktexteditor_insertfile)

install(FILES
  ktexteditor_insertfile.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(ktexteditor_insertfile PRIVATE "${PROJECT_SOURCE_DIR}/interfaces")
