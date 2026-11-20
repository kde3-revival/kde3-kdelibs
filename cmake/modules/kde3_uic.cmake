# Runs uic like admin/am_edit (tag_UIFILES, without KDE_OPTIONS=qtonly).
#
#   cmake -DUIC=<uic> -DUI_FILE=<foo.ui> -DHEADER=<foo.h> [-DPLUGIN_DIR=<dir>]
#         -P kde3_uic.cmake                     -> foo.h
#   cmake ... -DSOURCE=<foo.cpp> -DMANGLED=<foo> -P kde3_uic.cmake -> foo.cpp
#
# foo.h:   uic foo.ui, "public QWizard" -> "public KWizard",
#          "#include <qwizard.h>" -> "#include <kwizard.h>"
# foo.cpp: "#include <kdialog.h>" and "#include <klocale.h>", then
#          uic -tr tr2i18n -i foo.h foo.ui with tr2i18n( "" ) and
#          tr2i18n( "", "" ) replaced by QString::null, image<N>_data
#          renamed to img<N>_<mangled name> and ": QWizard(" ->
#          ": KWizard(", then "#include "foo.moc"".
#
# Output is written to a temporary file and renamed on success.

set(_args -nounload)
if(PLUGIN_DIR)
  # uic says nothing when it cannot load a plugin; it just leaves out
  # what it knows from it (the #includes of the KDE widgets).
  # KDE3LibsMacros checks at configure time that it loads; here only
  # that it is still there.
  if(NOT EXISTS "${PLUGIN_DIR}/designer")
    message(FATAL_ERROR "uic: no designer plugins in ${PLUGIN_DIR}/designer "
      "(KDE3_UIC_PLUGIN_DIR); the KDE widgets of ${UI_FILE} would be "
      "compiled without their declarations")
  endif()
  list(APPEND _args -L "${PLUGIN_DIR}")
endif()

if(NOT SOURCE)
  execute_process(COMMAND "${UIC}" ${_args} "${UI_FILE}"
    OUTPUT_VARIABLE _out RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "uic ${UI_FILE} failed: ${_result}")
  endif()
  string(REPLACE "public QWizard" "public KWizard" _out "${_out}")
  string(REPLACE "#include <qwizard.h>" "#include <kwizard.h>" _out "${_out}")
  file(WRITE "${HEADER}.tmp" "${_out}")
  file(RENAME "${HEADER}.tmp" "${HEADER}")
else()
  get_filename_component(_hname "${HEADER}" NAME)
  get_filename_component(_base "${HEADER}" NAME_WE)
  execute_process(COMMAND "${UIC}" ${_args} -tr tr2i18n -i "${_hname}" "${UI_FILE}"
    OUTPUT_VARIABLE _out RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "uic -i ${_hname} ${UI_FILE} failed: ${_result}")
  endif()
  string(REPLACE "tr2i18n( \"\" )" "QString::null" _out "${_out}")
  string(REPLACE "tr2i18n( \"\", \"\" )" "QString::null" _out "${_out}")
  string(REGEX REPLACE "image([0-9][0-9]*)_data" "img\\1_${MANGLED}" _out "${_out}")
  string(REPLACE ": QWizard(" ": KWizard(" _out "${_out}")
  file(WRITE "${SOURCE}.tmp"
    "#include <kdialog.h>\n#include <klocale.h>\n${_out}#include \"${_base}.moc\"\n")
  file(RENAME "${SOURCE}.tmp" "${SOURCE}")
endif()
