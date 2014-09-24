
find_package(SFML 2.1 COMPONENTS graphics window system audio REQUIRED)
find_package(OpenGL REQUIRED)

#XXX fix a segfault bug on some systems
if(UNIX AND CMAKE_COMPILER_IS_GNUCXX)
  list(INSERT OPENGL_LIBRARIES 0 stdc++)
endif()


# Set variables for parent scope

set(PNP_INTF_GUI_SRCS
  interface.cpp resources.cpp style.cpp screen.cpp widget.cpp animation.cpp
  screen_menus.cpp screen_game.cpp
  PARENT_SCOPE
  )

set(PNP_INTF_GUI_LIBRARIES
  ${SFML_LIBRARIES} ${SFML_DEPENDENCIES} ${OPENGL_LIBRARIES}
  PARENT_SCOPE
  )

set(PNP_INTF_GUI_INCLUDE_DIRS
  ${SFML_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR}
  PARENT_SCOPE
  )

if(SFML_STATIC_LIBRARIES)
  set(PNP_INTF_GUI_DEFS -DSFML_STATIC PARENT_SCOPE)
endif()

