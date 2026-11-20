kde3_add_executable(kioexec
  SOURCES
    main.cpp
  MOC
    main.h
  LINK
    kio
)
# install-exec-local
kde3_install_symlink(kioexec ${KDE3_BIN_INSTALL_DIR}/kfmexec)
