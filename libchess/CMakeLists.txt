add_library(chess STATIC
  pgn.c
  move.c
  chess.h
  common.h
)

if(ENABLE_DEBUG)
  target_sources(chess PRIVATE debug.c debug.h)
  target_compile_definitions(chess PUBLIC DEBUG)
endif()

# Compatibility shims when the host lacks them.
if(NOT HAVE_STRSEP)
  target_sources(chess PRIVATE ${CMAKE_SOURCE_DIR}/osdep/strsep.c)
endif()

if(NOT HAVE_ERR_H)
  target_sources(chess PRIVATE ${CMAKE_SOURCE_DIR}/osdep/err.c)
  target_include_directories(chess PUBLIC ${CMAKE_SOURCE_DIR}/osdep)
endif()

target_include_directories(chess
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    ${CMAKE_BINARY_DIR}
)

target_compile_definitions(chess PRIVATE HAVE_CONFIG_H)

set_target_properties(chess PROPERTIES
  OUTPUT_NAME chess
  POSITION_INDEPENDENT_CODE OFF
)
