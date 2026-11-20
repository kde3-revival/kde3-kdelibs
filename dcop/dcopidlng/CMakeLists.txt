install(PROGRAMS
  dcopidlng
  DESTINATION ${KDE3_BIN_INSTALL_DIR})

install(FILES
  Ast.pm
  Iter.pm
  kalyptusCxxToDcopIDL.pm
  kdocAstUtil.pm
  kdocParseDoc.pm
  kdocUtil.pm
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/dcopidlng)

install(PROGRAMS
  kalyptus
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/dcopidlng)
