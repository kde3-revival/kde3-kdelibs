# libkhtmlxml.la (convenience library of libkhtml)
kde3_add_library(khtmlxml OBJECT
  LINK
    kparts
    kutils
    kwalletclient
  SOURCES
    dom_docimpl.cpp
    dom_nodeimpl.cpp
    dom_textimpl.cpp
    dom_elementimpl.cpp
    dom_stringimpl.cpp
    dom2_rangeimpl.cpp
    dom2_traversalimpl.cpp
    xml_tokenizer.cpp
    dom_xmlimpl.cpp
    dom2_eventsimpl.cpp
    dom2_viewsimpl.cpp
    dom_restyler.cpp
  MOC
    dom_docimpl.h
    xml_tokenizer.h
)
