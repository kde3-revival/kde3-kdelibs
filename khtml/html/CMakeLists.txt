# libkhtmlhtml.la (convenience library of libkhtml)
kde3_add_library(khtmlhtml OBJECT
  LINK
    kparts
    kutils
    kwalletclient
  SOURCES
    htmlparser.cpp
    htmltokenizer.cpp
    dtd.cpp
    html_headimpl.cpp
    html_blockimpl.cpp
    html_elementimpl.cpp
    html_inlineimpl.cpp
    html_documentimpl.cpp
    html_baseimpl.cpp
    html_imageimpl.cpp
    html_listimpl.cpp
    html_miscimpl.cpp
    html_formimpl.cpp
    html_objectimpl.cpp
    html_tableimpl.cpp
  MOC
    html_documentimpl.h
    html_objectimpl.h
)
