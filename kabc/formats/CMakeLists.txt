kde3_add_module(kabcformat_binary
  SOURCES
    binaryformat.cpp
  LINK
    kabc
)

install(FILES
  binary.desktop
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kabc/formats)
