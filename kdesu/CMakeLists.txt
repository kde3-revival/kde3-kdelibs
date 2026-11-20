kde3_add_library(kdesu SHARED
  VERSION_INFO 6:0:2
  SOURCES
    client.cpp
    process.cpp
    kcookie.cpp
    su.cpp
    ssh.cpp
    stub.cpp
    kdesu_pty.cpp
  LINK
    kdecore
  LINK_PRIVATE
    ${KDE3_LIBUTIL}
)
target_include_directories(kdesu PRIVATE "${PROJECT_SOURCE_DIR}/kio")

kde3_add_executable(kdesu_stub
  SOURCES
    kdesu_stub.c
)

install(FILES
  defaults.h
  client.h
  process.h
  kdesu_pty.h
  kcookie.h
  su.h
  ssh.h
  stub.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kdesu)
# install-data-hook: kdesu/pty.h -> kdesu_pty.h
kde3_install_symlink(kdesu_pty.h ${KDE3_INCLUDE_INSTALL_DIR}/kdesu/pty.h)
