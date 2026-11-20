install(FILES
  jstest.desktop
  jstest.js
  script-indent-c-newline.js
  script-indent-c-char.js
  sort.js
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/katepart/scripts)

install(FILES
  script-indent-c-test.js
  script-indent-c1-test.lua
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/katepart/scripts/indent)
