# *.lut.h: hash tables generated with kjs/create_hash_table
set(_luts)
foreach(_name kjs_dom kjs_html kjs_css kjs_events kjs_navigator kjs_mozilla
              kjs_range kjs_traversal kjs_views kjs_window xmlhttprequest
              xmlserializer domparser)
  set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_name}.lut.h")
  kde3_generate_stdout("${_out}"
    COMMAND perl "${PROJECT_SOURCE_DIR}/kjs/create_hash_table"
            "${CMAKE_CURRENT_SOURCE_DIR}/${_name}.cpp"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_name}.cpp"
            "${PROJECT_SOURCE_DIR}/kjs/create_hash_table")
  list(APPEND _luts "${_out}")
endforeach()

# libkjs_html.la (convenience library of libkhtml)
kde3_add_library(kjs_html OBJECT
  LINK
    kparts
    kutils
    kwalletclient
    kjs
  SOURCES
    ${_luts}
    kjs_binding.cpp
    kjs_dom.cpp
    kjs_html.cpp
    kjs_window.cpp
    kjs_navigator.cpp
    kjs_proxy.cpp
    kjs_css.cpp
    kjs_range.cpp
    kjs_traversal.cpp
    kjs_events.cpp
    kjs_views.cpp
    kjs_debugwin.cpp
    kjs_mozilla.cpp
    xmlhttprequest.cpp
    xmlserializer.cpp
    domparser.cpp
  MOC
    kjs_debugwin.h
    kjs_window.h
    xmlhttprequest.h
  EXCEPTIONS
)
