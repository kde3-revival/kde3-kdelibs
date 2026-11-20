kde3_add_library(katepartinterfaces SHARED
  VERSION_INFO 0:0:0
  SOURCES
    interfaces.cpp
    katecmd.cpp
  MOC
    document.h
    view.h
  LINK
    ktexteditor
)

install(FILES
  document.h
  view.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kate)
target_include_directories(katepartinterfaces PRIVATE
  "${PROJECT_SOURCE_DIR}/interfaces" "${PROJECT_SOURCE_DIR}/kparts")
