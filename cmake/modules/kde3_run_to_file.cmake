# cmake -DOUTPUT=<file> -DCOMMAND=<cmd;arg;...> [-DINPUT=<file>]
#       -P kde3_run_to_file.cmake
#
# Runs a code generator that writes to stdout (the "> $@" of Makefile.am
# rules) and stores its output in OUTPUT.  The output goes to a temporary
# file that is renamed only on success, so a failed run never leaves a
# partial file behind.
set(_input)
if(INPUT)
  set(_input INPUT_FILE "${INPUT}")
endif()
execute_process(COMMAND ${COMMAND}
  ${_input}
  OUTPUT_FILE "${OUTPUT}.tmp"
  RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
  file(REMOVE "${OUTPUT}.tmp")
  string(REPLACE ";" " " _cmd "${COMMAND}")
  message(FATAL_ERROR "${_cmd} failed: ${_result}")
endif()
file(RENAME "${OUTPUT}.tmp" "${OUTPUT}")
