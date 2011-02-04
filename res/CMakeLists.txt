
set(GENERATE_RESOURCES FALSE CACHE BOOL "Generate resources")

if(NOT GENERATE_RESOURCES)
  return()
endif()

find_package(Inkscape)
if(NOT INKSCAPE_FOUND)
  message(SEND_ERROR "Inkscape not found, cannot generate resources.")
  return()
endif()


# In-game images
INKSCAPE_CONVERT_SVG2PNG(GAME_PNGS res.svg 
  DPI 45
  BkColor-map BkGarbage-map
  SwapCursor Labels
  GbWaiting-map Field-Frame
  )

# icon
foreach(size 16 32 64)
  INKSCAPE_CONVERT_SVG2PNG(ICON_PNGS res.svg APPEND OUTPUT icon-${size} WIDTH ${size} App-icon)
endforeach()

add_custom_target(resources ALL
  DEPENDS ${GAME_PNGS} ${ICON_PNGS}
  )

