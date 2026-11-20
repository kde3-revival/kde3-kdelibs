install(FILES
  index.theme
  DESTINATION ${KDE3_ICON_INSTALL_DIR}/crystalsvg)

# cr32-key_enter.png has no icon type in its name; am_edit skipped it
# ("unknown icon type cr32"), so it was never installed.
kde3_install_icons(AUTO EXCLUDE cr32-key_enter.png)
