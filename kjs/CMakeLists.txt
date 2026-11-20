configure_file(global.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/global.h")

# Hash tables for the static property lookup (create_hash_table > $@).
set(_lut_inputs
  lexer:keywords.table
  array_object:array_object.cpp
  math_object:math_object.cpp
  date_object:date_object.cpp
  number_object:number_object.cpp
  string_object:string_object.cpp)
set(_luts)
foreach(_pair IN LISTS _lut_inputs)
  string(REPLACE ":" ";" _pair "${_pair}")
  list(GET _pair 0 _name)
  list(GET _pair 1 _input)
  set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_name}.lut.h")
  kde3_generate_stdout("${_out}"
    COMMAND perl "${CMAKE_CURRENT_SOURCE_DIR}/create_hash_table"
            "${CMAKE_CURRENT_SOURCE_DIR}/${_input}" -i
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_input}"
            "${CMAKE_CURRENT_SOURCE_DIR}/create_hash_table")
  list(APPEND _luts "${_out}")
endforeach()

kde3_add_library(kjs SHARED
  VERSION_INFO 3:0:2
  SOURCES
    ustring.cpp
    date_object.cpp
    collector.cpp
    nodes.cpp
    grammar.cpp
    lexer.cpp
    lookup.cpp
    operations.cpp
    regexp.cpp
    function_object.cpp
    string_object.cpp
    bool_object.cpp
    number_object.cpp
    internal.cpp
    array_object.cpp
    math_object.cpp
    object_object.cpp
    regexp_object.cpp
    error_object.cpp
    function.cpp
    debugger.cpp
    value.cpp
    list.cpp
    object.cpp
    interpreter.cpp
    property_map.cpp
    nodes2string.cpp
    identifier.cpp
    reference.cpp
    reference_list.cpp
    scope_chain.cpp
    dtoa.cpp
    ${_luts}
  LINK_PRIVATE
    m
)
if(HAVE_PCREPOSIX)
  target_link_libraries(kjs PRIVATE PkgConfig::PCRE)
endif()
# include_VERSION_SCRIPT (GNU ld)
target_link_options(kjs PRIVATE
  "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libkjs.map")
set_property(TARGET kjs APPEND PROPERTY LINK_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/libkjs.map")

install(FILES
  value.h
  types.h
  object.h
  interpreter.h
  operations.h
  ustring.h
  lookup.h
  ${CMAKE_CURRENT_BINARY_DIR}/global.h
  identifier.h
  property_map.h
  reference.h
  reference_list.h
  completion.h
  scope_chain.h
  list.h
  simple_number.h
  function.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kjs)
