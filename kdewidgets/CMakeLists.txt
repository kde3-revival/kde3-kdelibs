kde3_add_executable(makekdewidgets
  EXPORT KDE3LibsTargets
  SOURCES
    makekdewidgets.cpp
  LINK
    KDE3Qt::Qt
    kdecore
)
add_executable(KDE3::makekdewidgets ALIAS makekdewidgets)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/kdewidgets.cpp"
  COMMAND makekdewidgets -o "${CMAKE_CURRENT_BINARY_DIR}/kdewidgets.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/kde.widgets"
  DEPENDS makekdewidgets "${CMAKE_CURRENT_SOURCE_DIR}/kde.widgets"
  COMMENT "Generating kdewidgets.cpp"
  VERBATIM)

kde3_add_module(kdewidgets
  DESTINATION ${KDE3_WIDGET_INSTALL_DIR}
  SOURCES
    classpreviews.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/kdewidgets.cpp
  LINK
    kio
    kabc
)
# AM_CXXFLAGS = -DQT_PLUGIN; INCLUDES = -I$(top_srcdir)/kfile
target_compile_definitions(kdewidgets PRIVATE QT_PLUGIN)
target_include_directories(kdewidgets PRIVATE "${PROJECT_SOURCE_DIR}/kfile")

add_subdirectory(pics)
