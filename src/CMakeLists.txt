set(CBOARD_SOURCES
  cboard.c
  message.c
  message.h
  misc.c
  misc.h
  engine.c
  engine.h
  input.c
  input.h
  rcfile.c
  rcfile.h
  colors.c
  colors.h
  window.c
  window.h
  conf.h
  common.h
  menu.c
  menu.h
  tags.c
  keys.h
  filebrowser.c
  filebrowser.h
  gettext.h
)

if(WITH_LIBPERL)
  list(APPEND CBOARD_SOURCES perl-plugin.c perl-plugin.h)
endif()

add_executable(cboard ${CBOARD_SOURCES})

target_include_directories(cboard
  PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/libchess
    ${CURSES_INCLUDE_DIRS}
)

target_compile_definitions(cboard
  PRIVATE
    HAVE_CONFIG_H
    DATA_PATH="${CMAKE_INSTALL_FULL_DATADIR}/cboard"
    LOCALE_DIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
)

if(ENABLE_DEBUG)
  target_compile_definitions(cboard PRIVATE DEBUG)
endif()

target_link_libraries(cboard
  PRIVATE
    chess
    ${CURSES_LIBRARIES}
    ${PANEL_LIBRARY}
    ${MENU_LIBRARY}
    ${FORM_LIBRARY}
)

if(CBOARD_HAS_NLS AND Intl_FOUND)
  target_link_libraries(cboard PRIVATE Intl::Intl)
endif()

if(WITH_LIBPERL)
  # Flags from ExtUtils::Embed may include -I/-D/-L/-l and -Wl options.
  # SHELL: keeps them as a single shell-tokenized group so -L paths apply.
  target_compile_options(cboard PRIVATE "SHELL:${LIBPERL_CFLAGS}")
  target_link_options(cboard PRIVATE "SHELL:${LIBPERL_LDFLAGS}")
endif()

install(TARGETS cboard RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
