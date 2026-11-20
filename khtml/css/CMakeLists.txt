# libkhtmlcss.la (convenience library of libkhtml)
kde3_add_library(khtmlcss OBJECT
  LINK
    kparts
    kutils
    kwalletclient
  SOURCES
    css_stylesheetimpl.cpp
    css_ruleimpl.cpp
    css_valueimpl.cpp
    css_base.cpp
    cssparser.cpp
    cssstyleselector.cpp
    csshelper.cpp
    parser.cpp
    css_renderstyledeclarationimpl.cpp
)

install(FILES
  html4.css
  quirks.css
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/khtml/css)
