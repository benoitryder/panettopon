# Locate Inkscape.
#
# Use the INKSCAPE_DIR CMake variable if inkscape is installed in a
# non-standard standard path.
#
# Define the following variables:
#   INKSCAPE_FOUND
#   INKSCAPE_EXECUTABLE  (cached)
#   INKSCAPE_DPI  (cached)
#
# Define the following functions:
#   INKSCAPE_CONVERT_SVG2PNG(
#     var              # variable set to the output filenames
#     svg              # SVG input file
#     [APPEND]         # append output filenames to var
#     [OUTPUT output]  # output name, without extension (only with one id)
#     [DPI dpi]        # export size ratio
#     [WIDTH width]    # export width
#     [HEIGHT height]  # export width
#     ids              # SVG object IDs to export
#   )


function(INKSCAPE_CONVERT_SVG2PNG var svg)
  # options
  set(arg_append FALSE)
  set(args_dpi)
  set(args_width)
  set(args_height)
  set(arg_output)
  while(ARGN)
    list(GET ARGN 0 a)
    if("${a}" STREQUAL "APPEND")
      set(arg_append TRUE)
      list(REMOVE_AT ARGN 0)
    elseif("${a}" STREQUAL "DPI")
      list(GET ARGN 1 v)
      set(args_dpi "-d ${v}")
      list(REMOVE_AT ARGN 0 1)
    elseif("${a}" STREQUAL "WIDTH")
      list(GET ARGN 1 v)
      set(args_width "-w ${v}")
      list(REMOVE_AT ARGN 0 1)
    elseif("${a}" STREQUAL "HEIGHT")
      list(GET ARGN 1 v)
      set(args_height "-h ${v}")
      list(REMOVE_AT ARGN 0 1)
    elseif("${a}" STREQUAL "OUTPUT")
      list(GET ARGN 1 arg_output)
      list(REMOVE_AT ARGN 0 1)
    else()
      break()
    endif()
  endwhile()

  if(arg_output)
    list(LENGTH ${ARGN} ids_len)
    if(ids_len GREATER 1)
      message(SEND_ERROR "Cannot use OUTPUT with multiple IDs")
    endif()
  endif()

  if(arg_append)
    set(pngs ${${var}})
  else()
    set(pngs)
  endif()
  get_filename_component(svg_abs ${svg} ABSOLUTE)

  if(EXISTS /dev/null)
    set(dev_null /dev/null)
  else()
    set(dev_null NUL)
  endif()
  foreach(id ${ARGN})
    if(arg_output)
      set(output_name ${arg_output})
    else()
      set(output_name ${id})
    endif()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${output_name}.png")
    list(APPEND pngs ${output})
    file(RELATIVE_PATH relative_output ${CMAKE_BINARY_DIR} ${output})
    add_custom_command(
      OUTPUT ${output}
      COMMAND ${INKSCAPE_EXECUTABLE}
        -z ${svg_abs} -e ${output} -j -i ${id} -b black -y 0
        ${args_dpi} ${args_width} ${args_height} > ${dev_null}
      DEPENDS ${svg_abs}
      COMMENT "Exporting SVG object to ${relative_output}"
      VERBATIM)
  endforeach()

  set(${var} ${pngs} PARENT_SCOPE)
endfunction()


if(WIN32)
  get_filename_component(_inkscape_reg_path
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\inkscape.exe]"
    PATH)
endif()
find_program(INKSCAPE_EXECUTABLE
  NAMES inkscape
  PATHS ${INKSCAPE_DIR} ${_inkscape_reg_path}
  )
mark_as_advanced(INKSCAPE_EXECUTABLE)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Inkscape DEFAULT_MSG INKSCAPE_EXECUTABLE)

