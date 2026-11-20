kde3_add_library(ktexteditor SHARED
  VERSION_INFO 0:0:0
  SOURCES
    ktexteditor.cpp
    editinterface.cpp
    clipboardinterface.cpp
    selectioninterface.cpp
    searchinterface.cpp
    codecompletioninterface.cpp
    wordwrapinterface.cpp
    blockselectioninterface.cpp
    configinterface.cpp
    cursorinterface.cpp
    dynwordwrapinterface.cpp
    printinterface.cpp
    highlightinginterface.cpp
    markinterface.cpp
    popupmenuinterface.cpp
    undointerface.cpp
    viewcursorinterface.cpp
    editdcopinterface.cpp
    editdcopinterface.skel
    clipboarddcopinterface.cpp
    clipboarddcopinterface.skel
    selectiondcopinterface.cpp
    selectiondcopinterface.skel
    searchdcopinterface.cpp
    searchdcopinterface.skel
    markinterfaceextension.cpp
    configinterfaceextension.cpp
    encodinginterface.cpp
    sessionconfiginterface.cpp
    viewstatusmsginterface.cpp
    editorchooser_ui.ui
    editorchooser.cpp
    blockselectiondcopinterface.cpp
    documentinfo.cpp
    documentdcopinfo.cpp
    blockselectiondcopinterface.skel
    documentdcopinfo.skel
    encodingdcopinterface.cpp
    encodingdcopinterface.skel
    printdcopinterface.cpp
    printdcopinterface.skel
    undodcopinterface.cpp
    undodcopinterface.skel
    viewcursordcopinterface.cpp
    viewcursordcopinterface.skel
    viewstatusmsgdcopinterface.cpp
    viewstatusmsgdcopinterface.skel
    selectioninterfaceext.cpp
    selectionextdcopinterface.skel
    texthintinterface.cpp
    editinterfaceext.cpp
    variableinterface.cpp
    templateinterface.cpp
  MOC
    configinterfaceextension.h
    document.h
    editor.h
    editorchooser.h
    plugin.h
    view.h
  LINK
    kparts
    DCOP
    kabc
)

install(FILES
  document.h
  view.h
  editor.h
  plugin.h
  editinterface.h
  undointerface.h
  selectioninterface.h
  cursorinterface.h
  clipboardinterface.h
  popupmenuinterface.h
  viewcursorinterface.h
  searchinterface.h
  highlightinginterface.h
  blockselectioninterface.h
  codecompletioninterface.h
  configinterface.h
  markinterface.h
  printinterface.h
  wordwrapinterface.h
  dynwordwrapinterface.h
  markinterfaceextension.h
  configinterfaceextension.h
  encodinginterface.h
  viewstatusmsginterface.h
  sessionconfiginterface.h
  editorchooser.h
  documentinfo.h
  selectioninterfaceext.h
  selectionextdcopinterface.h
  texthintinterface.h
  editinterfaceext.h
  variableinterface.h
  templateinterface.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/ktexteditor)

install(FILES
  ktexteditor.desktop
  ktexteditoreditor.desktop
  ktexteditorplugin.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

install(FILES
  kcm_ktexteditor.desktop
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kcm_componentchooser)
target_include_directories(ktexteditor PRIVATE
  "${PROJECT_SOURCE_DIR}/interfaces" "${PROJECT_BINARY_DIR}/interfaces"
  "${PROJECT_SOURCE_DIR}/kabc" "${PROJECT_BINARY_DIR}/kabc")
