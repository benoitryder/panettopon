
find_package(Inkscape REQUIRED)

# In-game images
INKSCAPE_CONVERT_SVG2PNG(GAME_PNGS res.svg 
  DPI 45
  BkColor-map BkGarbage-map
  SwapCursor Signs
  GbHanging-map Field-Frame
  Menu-map Menu-Background
  )

# icon
foreach(size 16 32 64 128)
  INKSCAPE_CONVERT_SVG2PNG(ICON_PNGS res.svg APPEND OUTPUT icon-${size} WIDTH ${size} App-icon)
endforeach()

add_custom_target(resources ALL
  DEPENDS ${GAME_PNGS} ${ICON_PNGS}
  )

if(WIN32)
  include(${CMAKE_SOURCE_DIR}/cmake/FindPythonModule.cmake)
  find_python_module(PIL REQUIRED)

  add_custom_command(
    OUTPUT panettopon.ico
    COMMAND python
    ARGS ${CMAKE_CURRENT_SOURCE_DIR}/images2ico.py panettopon.ico ${ICON_PNGS}
    DEPENDS ${ICON_PNGS}
    COMMENT "Creating .ico from PNGs"
    VERBATIM)

  add_custom_target(icon DEPENDS panettopon.ico)
  add_dependencies(resources icon)
endif()


#TODO install in share/ on Linux
install(FILES ${GAME_PNGS} ${ICON_PNGS} style.ini FreeSans.ttf DESTINATION res/)
install(DIRECTORY lang/ DESTINATION res/lang/ FILES_MATCHING PATTERN "*.ini")

