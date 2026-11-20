kde3_add_module(ktexteditor_docwordcompletion
  SOURCES
    docwordcompletion.cpp
  MOC
    docwordcompletion.h
  LINK
    ktexteditor
)

install(FILES
  docwordcompletionui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/ktexteditor_docwordcompletion)

install(FILES
  ktexteditor_docwordcompletion.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_include_directories(ktexteditor_docwordcompletion PRIVATE "${PROJECT_SOURCE_DIR}/interfaces")
