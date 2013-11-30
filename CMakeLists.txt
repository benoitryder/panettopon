cmake_minimum_required(VERSION 2.8)

project(Panettopon)


##
##  General declaration
##

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=gnu++11")
  if(BUILD_STATIC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
  endif()
endif()

if(WIN32)
  #XXX Windows version should be checked
  add_definitions(
    -D_WIN32_WINNT=0x501
    -DNOGDI  # avoid declaration of conflicting macros
    )
endif()


##
##  Targets
##

add_subdirectory(src)
add_subdirectory(res)

