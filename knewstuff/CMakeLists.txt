kde3_add_library(knewstuff SHARED
  VERSION_INFO 1:0
  SOURCES
    engine.cpp
    entry.cpp
    downloaddialog.cpp
    uploaddialog.cpp
    providerdialog.cpp
    provider.cpp
    knewstuff.cpp
    knewstuffgeneric.cpp
    knewstuffbutton.cpp
    knewstuffsecure.cpp
    security.cpp
    ocs.cpp
    json.cpp
    wallpapersources.cpp
    dailypicture.cpp
  MOC
    downloaddialog.h
    engine.h
    knewstuffbutton.h
    knewstuffsecure.h
    provider.h
    providerdialog.h
    security.h
    uploaddialog.h
    ocs.h
    wallpapersources.h
    dailypicture.h
  LINK
    kio
)

# ghns (EXTRA_PROGRAMS) is not built; am_edit compiled the moc of its
# ghns.h into libknewstuff, which is not needed.
kde3_add_executable(khotnewstuff
  SOURCES
    khotnewstuff.cpp
  LINK
    knewstuff
)

install(FILES
  types
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/knewstuff)

install(FILES
  khotnewstuffrc
  DESTINATION ${KDE3_CONFIG_INSTALL_DIR})

install(FILES
  downloaddialog.h
  engine.h
  entry.h
  knewstuffgeneric.h
  knewstuff.h
  providerdialog.h
  provider.h
  uploaddialog.h
  knewstuffbutton.h
  knewstuffsecure.h
  dailypicture.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/knewstuff)

kde3_install_icons(AUTO)
