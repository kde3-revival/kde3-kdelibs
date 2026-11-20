# undefined_symbols_allowed (everywhere but Cygwin): shared library
kde3_add_library(kscreensaver SHARED
  VERSION_INFO 6:0:2
  SOURCES
    main.cpp
    kscreensaver.cpp
  MOC
    kscreensaver.h
  LINK
    kio
)
# The screen savers provide kss_create() etc. (undefined_symbols_allowed)
target_link_options(kscreensaver PRIVATE "LINKER:-z,undefs")

install(FILES
  kscreensaver.h
  kscreensaver_vroot.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})
