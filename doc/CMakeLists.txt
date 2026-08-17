# Substitute package version into the man page template.
set(VERSION "${PROJECT_VERSION}")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/llama-chess.6.in
  ${CMAKE_CURRENT_BINARY_DIR}/llama-chess.6
  @ONLY
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/llama-chess.6
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man6
)

install(
  FILES config.example
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
