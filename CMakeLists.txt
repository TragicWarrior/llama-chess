cmake_minimum_required(VERSION 3.16)

project(cboard
  VERSION 0.7.6
  DESCRIPTION "Console/Curses PGN browser, editor and chess engine interface"
  LANGUAGES C
)

set(CBOARD_BUGREPORT "Ben Kibbey <bjk@luxsci.net>")
set(CBOARD_URL "https://gitlab.com/bjk/cboard/wikis")
# Match historical package version string used in --version output.
set(PROJECT_VERSION "${PROJECT_VERSION}-dev")

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CMakePushCheckState)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(WITH_LIBPERL "Enable optional PERL plugin support" OFF)
option(ENABLE_DEBUG "Enable debugging features (-DDEBUG)" OFF)
option(ENABLE_NLS "Enable native language support (gettext)" ON)

if(APPLE)
  add_link_options(-flat_namespace)
  # Fink / legacy paths occasionally used on macOS.
  list(APPEND CMAKE_PREFIX_PATH /sw)
endif()

# ---------------------------------------------------------------------------
# Curses (prefer wide-character ncurses)
# ---------------------------------------------------------------------------
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses)
if(NOT CURSES_FOUND)
  set(CURSES_NEED_WIDE FALSE)
  find_package(Curses REQUIRED)
endif()

# Panel / menu / form companion libraries (wide variants preferred).
find_library(PANEL_LIBRARY NAMES panelw panel REQUIRED)
find_library(MENU_LIBRARY NAMES menuw menu REQUIRED)
find_library(FORM_LIBRARY NAMES formw form REQUIRED)

# ---------------------------------------------------------------------------
# Header checks
# ---------------------------------------------------------------------------
check_include_file("dirent.h" HAVE_DIRENT_H)
check_include_file("err.h" HAVE_ERR_H)
check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("getopt.h" HAVE_GETOPT_H)
check_include_file("glob.h" HAVE_GLOB_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("limits.h" HAVE_LIMITS_H)
check_include_file("paths.h" HAVE_PATHS_H)
check_include_file("regex.h" HAVE_REGEX_H)
check_include_file("stdarg.h" HAVE_STDARG_H)
check_include_file("stdbool.h" HAVE_STDBOOL_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdio.h" HAVE_STDIO_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/param.h" HAVE_SYS_PARAM_H)
check_include_file("sys/socket.h" HAVE_SYS_SOCKET_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/time.h" HAVE_SYS_TIME_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("sys/wait.h" HAVE_SYS_WAIT_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("wchar.h" HAVE_WCHAR_H)

# Curses header layout (order mirrors src/common.h).
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES ${CURSES_INCLUDE_DIRS})
check_include_file("ncursesw/curses.h" HAVE_NCURSESW_CURSES_H)
check_include_file("ncursesw.h" HAVE_NCURSESW_H)
check_include_file("ncurses/curses.h" HAVE_NCURSES_CURSES_H)
check_include_file("ncurses.h" HAVE_NCURSES_H)
check_include_file("curses.h" HAVE_CURSES_H)

check_include_file("ncursesw/panel.h" HAVE_NCURSESW_PANEL_H)
check_include_file("ncurses/panel.h" HAVE_NCURSES_PANEL_H)
check_include_file("panel.h" HAVE_PANEL_H)

check_include_file("ncursesw/menu.h" HAVE_NCURSESW_MENU_H)
check_include_file("ncurses/menu.h" HAVE_NCURSES_MENU_H)
check_include_file("menu.h" HAVE_MENU_H)

check_include_file("ncursesw/form.h" HAVE_NCURSESW_FORM_H)
check_include_file("ncurses/form.h" HAVE_NCURSES_FORM_H)
check_include_file("form.h" HAVE_FORM_H)
cmake_pop_check_state()

if(HAVE_NCURSESW_CURSES_H OR HAVE_NCURSESW_H)
  set(HAVE_NCURSESW 1)
endif()
if(HAVE_NCURSES_CURSES_H OR HAVE_NCURSES_H OR HAVE_NCURSESW)
  set(HAVE_NCURSES 1)
endif()
if(HAVE_CURSES_H OR HAVE_NCURSES OR HAVE_NCURSESW)
  set(HAVE_CURSES 1)
endif()
if(HAVE_NCURSESW_PANEL_H OR HAVE_NCURSES_PANEL_H OR HAVE_PANEL_H)
  set(HAVE_PANEL 1)
endif()
if(HAVE_NCURSESW_MENU_H OR HAVE_NCURSES_MENU_H OR HAVE_MENU_H)
  set(HAVE_MENU 1)
endif()
if(HAVE_NCURSESW_FORM_H OR HAVE_NCURSES_FORM_H OR HAVE_FORM_H)
  set(HAVE_FORM 1)
endif()

if(NOT HAVE_CURSES)
  message(FATAL_ERROR "A SysV or X/Open-compatible Curses library is required")
endif()
if(NOT HAVE_PANEL)
  message(FATAL_ERROR "Curses panel library/headers not found")
endif()
if(NOT HAVE_MENU)
  message(FATAL_ERROR "Curses menu library/headers not found")
endif()
if(NOT HAVE_FORM)
  message(FATAL_ERROR "Curses form library/headers not found")
endif()

# ---------------------------------------------------------------------------
# Function checks
# ---------------------------------------------------------------------------
foreach(_fn
    bzero dup2 fork getcwd gethostbyname memset mkdir mkfifo mkstemp putenv
    regcomp select socket strcasecmp strchr strdup strerror strftime
    strncasecmp strpbrk strrchr strsep strstr vasprintf vprintf)
  string(TOUPPER "${_fn}" _fn_upper)
  check_function_exists(${_fn} HAVE_${_fn_upper})
endforeach()

# GLOB_NOMATCH
if(HAVE_GLOB_H)
  check_c_source_compiles([[
    #include <glob.h>
    int main(void) { int n = GLOB_NOMATCH; (void)n; return 0; }
  ]] HAVE_GLOB_NOMATCH)
endif()

# __progname (BSD/Linux)
if(HAVE_ERR_H)
  check_c_source_compiles([[
    #include <stdio.h>
    extern char *__progname;
    int main(void) { return __progname != 0; }
  ]] HAVE_PROGNAME)
endif()

# ---------------------------------------------------------------------------
# Unix98 PTY support
# ---------------------------------------------------------------------------
if(EXISTS "/dev/ptmx")
  if(CMAKE_CROSSCOMPILING)
    set(UNIX98 1)
  else()
    check_c_source_runs([[
      #define _GNU_SOURCE
      #include <stdlib.h>
      #include <fcntl.h>
      int main(void) {
        int fd = open("/dev/ptmx", O_RDWR);
        if (fd == -1) return 1;
        if (ptsname(fd) == NULL) return 1;
        return 0;
      }
    ]] UNIX98)
  endif()
endif()

if(NOT UNIX98)
  # Legacy BSD-style pty device ranges (from GNU Screen / former configure.ac).
  if(IS_DIRECTORY "/dev/ptym")
    set(_pty_dir "/dev/ptym")
  else()
    set(_pty_dir "/dev")
  endif()
  file(GLOB _pty_nodes "${_pty_dir}/pty??")
  if(_pty_nodes)
    set(_majors "")
    set(_minors "")
    foreach(_node ${_pty_nodes})
      get_filename_component(_base "${_node}" NAME)
      string(SUBSTRING "${_base}" 3 1 _maj)
      string(SUBSTRING "${_base}" 4 1 _min)
      string(APPEND _majors "${_maj}")
      string(APPEND _minors "${_min}")
    endforeach()
    # Unique-sort characters into PTY_MAJOR / PTY_MINOR strings.
    string(REGEX REPLACE "(.)" "\\1;" _maj_list "${_majors}")
    string(REGEX REPLACE "(.)" "\\1;" _min_list "${_minors}")
    list(REMOVE_DUPLICATES _maj_list)
    list(REMOVE_DUPLICATES _min_list)
    list(SORT _maj_list)
    list(SORT _min_list)
    string(JOIN "" PTY_MAJOR ${_maj_list})
    string(JOIN "" PTY_MINOR ${_min_list})
  endif()
endif()

# ---------------------------------------------------------------------------
# Optional PERL support
# ---------------------------------------------------------------------------
set(LIBPERL_CFLAGS "")
set(LIBPERL_LDFLAGS "")
if(WITH_LIBPERL)
  find_package(Perl REQUIRED)
  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MExtUtils::Embed -e ccopts
    OUTPUT_VARIABLE LIBPERL_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _perl_ccopts_rc
  )
  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MExtUtils::Embed -e ldopts
    OUTPUT_VARIABLE LIBPERL_LDFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _perl_ldopts_rc
  )
  if(NOT _perl_ccopts_rc EQUAL 0 OR NOT _perl_ldopts_rc EQUAL 0)
    message(FATAL_ERROR "Missing or incomplete PERL installation (ExtUtils::Embed)")
  endif()
  string(STRIP "${LIBPERL_CFLAGS}" LIBPERL_CFLAGS)
  string(STRIP "${LIBPERL_LDFLAGS}" LIBPERL_LDFLAGS)

  # Verify we can actually link against libperl (needs libperl-dev / equivalent).
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "${LIBPERL_CFLAGS}")
  set(CMAKE_REQUIRED_LIBRARIES "${LIBPERL_LDFLAGS}")
  check_c_source_compiles([[
    #include <EXTERN.h>
    #include <perl.h>
    int main(void) { return 0; }
  ]] HAVE_LIBPERL)
  cmake_pop_check_state()
  if(NOT HAVE_LIBPERL)
    message(FATAL_ERROR
      "WITH_LIBPERL=ON but libperl development files were not found.\n"
      "Install libperl-dev (Debian/Ubuntu) or the equivalent package.")
  endif()

  set(WITH_LIBPERL 1)
  message(STATUS "PERL support: enabled")
else()
  unset(WITH_LIBPERL)
  message(STATUS "PERL support: disabled (use -DWITH_LIBPERL=ON)")
endif()

# ---------------------------------------------------------------------------
# Gettext / NLS
# ---------------------------------------------------------------------------
set(CBOARD_HAS_NLS FALSE)
if(ENABLE_NLS)
  find_package(Intl)
  find_package(Gettext)
  if(Intl_FOUND)
    set(ENABLE_NLS 1)
    set(CBOARD_HAS_NLS TRUE)
  else()
    message(WARNING "libintl not found; building without NLS")
    unset(ENABLE_NLS)
  endif()
else()
  unset(ENABLE_NLS)
endif()

# ---------------------------------------------------------------------------
# Generate config.h
# ---------------------------------------------------------------------------
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  @ONLY
)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(libchess)
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(doc)

if(CBOARD_HAS_NLS AND GETTEXT_FOUND)
  # Compile message catalogs without modifying source .po files.
  # Install as share/locale/<lang>/LC_MESSAGES/cboard.mo
  set(_linguas)
  if(EXISTS "${CMAKE_SOURCE_DIR}/po/LINGUAS")
    file(STRINGS "${CMAKE_SOURCE_DIR}/po/LINGUAS" _linguas_raw)
    foreach(_lang ${_linguas_raw})
      string(STRIP "${_lang}" _lang)
      if(_lang AND NOT _lang MATCHES "^#")
        list(APPEND _linguas "${_lang}")
      endif()
    endforeach()
  endif()

  foreach(_lang ${_linguas})
    set(_po "${CMAKE_SOURCE_DIR}/po/${_lang}.po")
    if(NOT EXISTS "${_po}")
      continue()
    endif()
    set(_gmo "${CMAKE_BINARY_DIR}/po/${_lang}.gmo")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/po")
    add_custom_command(
      OUTPUT "${_gmo}"
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o "${_gmo}" "${_po}"
      DEPENDS "${_po}"
      COMMENT "Generating ${_lang}.gmo"
      VERBATIM
    )
    add_custom_target(cboard_mo_${_lang} ALL DEPENDS "${_gmo}")
    install(
      FILES "${_gmo}"
      DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${_lang}/LC_MESSAGES"
      RENAME cboard.mo
    )
  endforeach()
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "CBoard ${PROJECT_VERSION}")
message(STATUS "  Curses:     ${CURSES_LIBRARIES}")
message(STATUS "  Panel:      ${PANEL_LIBRARY}")
message(STATUS "  Menu:       ${MENU_LIBRARY}")
message(STATUS "  Form:       ${FORM_LIBRARY}")
message(STATUS "  PERL:       ${WITH_LIBPERL}")
message(STATUS "  Debug:      ${ENABLE_DEBUG}")
message(STATUS "  NLS:        ${CBOARD_HAS_NLS}")
message(STATUS "  Unix98 PTY: ${UNIX98}")
message(STATUS "  Prefix:     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Data dir:   ${CMAKE_INSTALL_FULL_DATADIR}/cboard")
message(STATUS "  Locale dir: ${CMAKE_INSTALL_FULL_LOCALEDIR}")
message(STATUS "")
