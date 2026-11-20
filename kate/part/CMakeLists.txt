# katejscript.lut.h
kde3_generate_stdout("${CMAKE_CURRENT_BINARY_DIR}/katejscript.lut.h"
  COMMAND perl "${PROJECT_SOURCE_DIR}/kjs/create_hash_table"
          "${CMAKE_CURRENT_SOURCE_DIR}/katejscript.cpp"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/katejscript.cpp"
          "${PROJECT_SOURCE_DIR}/kjs/create_hash_table")

# INCLUDES; $(top_builddir)/kjs before kio/kio of all_includes (global.h)
include_directories(BEFORE "${PROJECT_BINARY_DIR}/kjs")
include_directories(
  "${PROJECT_SOURCE_DIR}/kate/interfaces"
  "${PROJECT_SOURCE_DIR}/kdeprint"
  "${PROJECT_SOURCE_DIR}/interfaces"
  "${PROJECT_SOURCE_DIR}/interfaces/kregexpeditor"
  "${PROJECT_SOURCE_DIR}/kutils")

# libkate.la (convenience library of libkatepart)
kde3_add_library(kate OBJECT
  LINK
    kparts
    kdeprint
    kutils
    kjs
    ktexteditor
  SOURCES
    katesearch.cpp
    katebuffer.cpp
    katecmds.cpp
    kateundo.cpp
    katecursor.cpp
    katedialogs.cpp
    katedocument.cpp
    katefactory.cpp
    katehighlight.cpp
    katesyntaxdocument.cpp
    katetextline.cpp
    kateview.cpp
    kateconfig.cpp
    kateviewhelpers.cpp
    katecodecompletion.cpp
    katedocumenthelpers.cpp
    katecodefoldinghelpers.cpp
    kateviewinternal.cpp
    katebookmarks.cpp
    kateprinter.cpp
    katefont.cpp
    katelinerange.cpp
    katesupercursor.cpp
    katearbitraryhighlight.cpp
    katerenderer.cpp
    kateattribute.cpp
    kateautoindent.cpp
    katefiletype.cpp
    kateschema.cpp
    katedocument.skel
    katetemplatehandler.cpp
    katejscript.cpp
    katespell.cpp
    kateindentscriptabstracts.cpp
    kateluaindentscript.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/katejscript.lut.h
  MOC
    katearbitraryhighlight.h
    kateautoindent.h
    katebookmarks.h
    katebuffer.h
    katecodecompletion.h
    katecodefoldinghelpers.h
    katedialogs.h
    katedocument.h
    katedocumenthelpers.h
    katefiletype.h
    katehighlight.h
    kateprinter.h
    kateschema.h
    katesearch.h
    katespell.h
    katesupercursor.h
    katetemplatehandler.h
    kateview.h
    kateviewhelpers.h
    kateviewinternal.h
)

file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp" CONTENT "")
kde3_add_module(libkatepart
  SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp
  LINK
    kate
    katepartinterfaces
    kdeprint
    kutils
    kjs
)
