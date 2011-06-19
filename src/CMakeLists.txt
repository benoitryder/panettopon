
##
##  Dependencies
##


# Boost
set(Boost_FIND_REQUIRED 1)
find_package(Boost COMPONENTS system)
add_definitions(
  ${BOOST_DEFINITIONS}
  )
if(WIN32 AND Boost_USE_STATIC_LIBS)
  add_definitions(-DBOOST_PYTHON_STATIC_LIB)
endif()

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
list(APPEND PNP_LIBS ${Boost_LIBRARIES})

# Protobuf
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})
link_directories(${PROTOBUF_LIBRARY_DIRS})
list(APPEND PNP_LIBS ${PROTOBUF_LIBRARIES})

# DSO linking changes may require to link with some "extra" libraries
include(CheckFunctionExists)
include(CheckLibraryExists)

check_function_exists("clock_gettime" HAVE_CLOCK_GETTIME)
if(NOT HAVE_CLOCK_GETTIME)
  check_library_exists("rt" "clock_gettime" "" HAVE_RT_CLOCK_GETTIME)
  if(HAVE_RT_CLOCK_GETTIME)
    list(APPEND PNP_LIBS "-lrt")
  endif()
endif()

check_function_exists("pthread_getspecific" HAVE_PTHREAD_GETSPECIFIC)
if(NOT HAVE_PTHREAD_GETSPECIFIC)
  check_library_exists("pthread" "pthread_getspecific" "" HAVE_PTHREAD_PTHREAD_GETSPECIFIC)
  if(HAVE_PTHREAD_PTHREAD_GETSPECIFIC)
    list(APPEND PNP_LIBS "-lpthread")
  endif()
endif()



##
##  Interfaces
##

# Add definitions for a new interface with given source files.
# src is a source file or a directory.
# If it is a directory, the following variables may be defined:
#   PNP_INTF_<NAME>_SRCS
#   PNP_INTF_<NAME>_DEFS
#   PNP_INTF_<NAME>_INCLUDE_DIRS
#   PNP_INTF_<NAME>_LIBRARIES
#
# Define the following variables:
#   ENABLE_INTF_<NAME>  (configurable)
#   PNP_INTF_SRCS
#   PNP_INTF_DEFS,
#   PNP_INTF_ENABLED.
macro(PNP_ADD_INTERFACE ifname src)
  string(TOUPPER "${ifname}" ifname_upper)
  set(ENABLE_INTF_${ifname_upper} TRUE CACHE BOOL "Enable interface ${ifname}.")
  if(ENABLE_INTF_${ifname_upper})
    #message("pouet: ${PNP_INTF_${ifname_upper}_SRCS}")
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
      add_subdirectory(${src})
      foreach(f IN ITEMS ${PNP_INTF_${ifname_upper}_SRCS})
        list(APPEND PNP_INTF_SRCS "${src}/${f}")
      endforeach()
      add_definitions(${PNP_INTF_${ifname_upper}_DEFS})
      include_directories(${PNP_INTF_${ifname_upper}_INCLUDE_DIRS})
      list(APPEND PNP_LIBS ${PNP_INTF_${ifname_upper}_LIBRARIES})
    else()
      list(APPEND PNP_INTF_SRCS ${src})
    endif()
    set(PNP_INTF_ENABLED TRUE)
    message(STATUS "Interface ${ifname}: enabled")
  else()
    add_definitions(-DWITHOUT_INTF_${ifname_upper})
    message(STATUS "Interface ${ifname}: disabled")
  endif()
endmacro()


# Server interface
PNP_ADD_INTERFACE(server intf_server.cpp)

# Curses interface
PNP_ADD_INTERFACE(curses intf_curses.cpp)
if(ENABLE_INTF_CURSES)
  find_package(Curses)
  list(APPEND PNP_LIBS ${CURSES_LIBRARIES})
  include_directories(CURSES_INCLUDE_DIR)
endif()

# GUI interface
PNP_ADD_INTERFACE(gui gui)


# at least one property must be defined
if(NOT PNP_INTF_ENABLED)
  message(SEND_ERROR "No interface enabled")
endif()


##
##  Project targets
##

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS netplay.proto)

# Windows: .rc/.res for the .exe icon
if(WIN32)
  set(ICON_OBJ panettopon.rc)
  if(MINGW)
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
  endif(MINGW)
  set_source_files_properties(${ICON_OBJ} PROPERTIES
    OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/res/panettopon.ico
    )
else()
  set(ICON_OBJ)
endif()

add_executable(panettopon
  main.cpp
  instance.cpp client.cpp server.cpp netplay.cpp game.cpp
  config.cpp log.cpp optget.cpp
  ${PNP_INTF_SRCS}
  ${PROTO_SRCS} ${PROTO_HDRS}
  ${ICON_OBJ}
  )
target_link_libraries(panettopon ${PNP_LIBS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})  # workaround for some .h dependency
include_directories(${CMAKE_CURRENT_BINARY_DIR})  # for *.pb.h

if(WIN32)
  add_dependencies(panettopon icon)
  target_link_libraries(panettopon ws2_32 mswsock)
endif()

