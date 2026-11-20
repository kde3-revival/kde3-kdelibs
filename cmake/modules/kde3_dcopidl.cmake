# cmake -DDCOPIDL=<dcopidl> -DINPUT=<header> -DOUTPUT=<file.kidl> -P kde3_dcopidl.cmake
#
# dcopidl writes the IDL to stdout; write it to a temporary file and
# rename it on success so that a failure never leaves a partial .kidl.
execute_process(COMMAND "${DCOPIDL}" "${INPUT}"
  OUTPUT_FILE "${OUTPUT}.tmp"
  RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
  file(REMOVE "${OUTPUT}.tmp")
  message(FATAL_ERROR "dcopidl ${INPUT} failed: ${_result}")
endif()
file(RENAME "${OUTPUT}.tmp" "${OUTPUT}")
