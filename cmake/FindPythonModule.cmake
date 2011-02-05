# Locate Python modules.
#
# find_python_module(module1 ... [REQUIRED])
#   Find module with the given name and set PYMODULE_<name>_FOUND.

find_package(PythonInterp REQUIRED)

include(FindPackageHandleStandardArgs)

function(find_python_module)
  set(mod_required FALSE)
  list(GET ARGN -1 _alast)
  if(_alast STREQUAL "REQUIRED")
    list(REMOVE_AT ARGN -1)
    set(mod_required TRUE)
  endif()
  foreach(mod ${ARGN})
    if(mod_required)
      set(PYMODULE_${mod}_FIND_REQUIRED)
    endif()
    execute_process(
      COMMAND "${PYTHON_EXECUTABLE}" "-c" "import ${mod}"
      RESULT_VARIABLE _status OUTPUT_QUIET ERROR_QUIET
      )
    set(PYMODULE_${mod}_FOUND FALSE CACHE "Whether the Python module ${mod} is found" BOOL)
    if(_status EQUAL 0)
      FIND_PACKAGE_MESSAGE(PYMODULE_${mod} "Python module ${mod}: found" " ")
      set(PYMODULE_${mod}_FOUND TRUE)
    else()
      FIND_PACKAGE_MESSAGE(PYMODULE_${mod} "Python module ${mod}: not found" " ")
      if(mod_required)
        message(SEND_ERROR "Could not find Python module ${mod}")
      endif()
    endif()
    mark_as_advanced(PYMODULE_${mod})
  endforeach()
endfunction()


