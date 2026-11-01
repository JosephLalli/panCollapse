if(NOT DEFINED PROGRAM)
  message(FATAL_ERROR "PROGRAM is required")
endif()
execute_process(COMMAND "${PROGRAM}" --help RESULT_VARIABLE status
  OUTPUT_VARIABLE help ERROR_VARIABLE error)
if(NOT status EQUAL 0 OR NOT help MATCHES "Supported workflow: panCollapse index -> panCollapse count"
    OR NOT help MATCHES "Deprecated BAM conversion: panCollapse convert")
  message(FATAL_ERROR "help must distinguish native workflow from deprecated conversion: ${help}${error}")
endif()
# Missing inputs must fail, but first issue the deprecation warning. No graph or
# GAMP is opened by this check; existing BAM-output tests cover conversion.
execute_process(COMMAND "${PROGRAM}" convert RESULT_VARIABLE status
  OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(status EQUAL 0 OR NOT error MATCHES "warning: 'convert' is deprecated")
  message(FATAL_ERROR "explicit conversion must identify itself as deprecated: ${output}${error}")
endif()
