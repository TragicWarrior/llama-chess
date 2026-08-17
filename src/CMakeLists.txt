set(LLAMA_CHESS_SOURCES
  llama-chess.c
  message.c
  message.h
  misc.c
  misc.h
  engine.c
  engine.h
  ollama.c
  ollama.h
  input.c
  input.h
  rcfile.c
  rcfile.h
  colors.c
  colors.h
  window.c
  window.h
  ui_screen.c
  ui_screen.h
  conf.h
  common.h
  menu.c
  menu.h
  menubar.c
  menubar.h
  splash.c
  splash.h
  mouse.c
  mouse.h
  tags.c
  keys.h
  filebrowser.c
  filebrowser.h
  gettext.h
)

if(WITH_LIBPERL)
  list(APPEND LLAMA_CHESS_SOURCES perl-plugin.c perl-plugin.h)
endif()

add_executable(llama-chess ${LLAMA_CHESS_SOURCES})

target_include_directories(llama-chess
  PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/libchess
    ${CURSES_INCLUDE_DIRS}
)
# VDK headers: SYSTEM so toolkit warnings don't drown ours.
target_include_directories(llama-chess SYSTEM PRIVATE
  ${LIBVIPER_SOURCE_DIR}/vdk
  ${LIBVIPER_SOURCE_DIR}/vkmio
)

target_compile_definitions(llama-chess
  PRIVATE
    HAVE_CONFIG_H
    DATA_PATH="${CMAKE_INSTALL_FULL_DATADIR}/llama-chess"
    LOCALE_DIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
)

if(ENABLE_DEBUG)
  target_compile_definitions(llama-chess PRIVATE DEBUG)
endif()

target_link_libraries(llama-chess
  PRIVATE
    chess
    ${CURSES_LIBRARIES}
    "${VDK_LIBRARY}"
)

# Ensure the build tree finds sibling libvdk.so without install.
set_target_properties(llama-chess PROPERTIES
  BUILD_RPATH "${LIBVIPER_SOURCE_DIR}"
  INSTALL_RPATH_USE_LINK_PATH TRUE
)

if(CBOARD_HAS_NLS AND Intl_FOUND)
  target_link_libraries(llama-chess PRIVATE Intl::Intl)
endif()

if(WITH_LIBPERL)
  # Flags from ExtUtils::Embed may include -I/-D/-L/-l and -Wl options.
  # SHELL: keeps them as a single shell-tokenized group so -L paths apply.
  target_compile_options(llama-chess PRIVATE "SHELL:${LIBPERL_CFLAGS}")
  target_link_options(llama-chess PRIVATE "SHELL:${LIBPERL_LDFLAGS}")
endif()

install(TARGETS llama-chess RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
