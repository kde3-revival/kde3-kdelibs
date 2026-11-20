kde3_add_executable(kdeinit
  SOURCES
    kinit.cpp
    setproctitle.cpp
  LINK
    kparts
)
if(KDEINIT_USE_XFT)
  target_link_libraries(kdeinit PRIVATE X11::Xft Freetype::Freetype)
endif()
if(KDEINIT_USE_FONTCONFIG)
  target_link_libraries(kdeinit PRIVATE Fontconfig::Fontconfig)
endif()

# wrapper.c behaves according to its program name
foreach(_prog kdeinit_wrapper kdeinit_shutdown)
  kde3_add_executable(${_prog}
    SOURCES
      wrapper.c
  )
endforeach()

kde3_add_executable(kshell
  SOURCES
    shell.c
)

kde3_add_executable(lnusertemp
  SOURCES
    lnusertemp.c
)

kde3_add_executable(kwrapper
  SOURCES
    kwrapper.c
)

kde3_add_executable(kioslave
  SOURCES
    kioslave.cpp
  LINK
    kio
)

kde3_add_executable(kstartupconfig
  SOURCES
    kstartupconfig.cpp
)

kde3_add_executable(kdostartupconfig
  SOURCES
    kdostartupconfig.cpp
  LINK
    kdecore
)

kde3_add_executable(start_kdeinit
  SOURCES
    start_kdeinit.c
)
target_compile_definitions(start_kdeinit PRIVATE
  "EXECUTE=\"${KDE3_FULL_BIN_INSTALL_DIR}/kdeinit\"")
set_target_properties(start_kdeinit PROPERTIES POSITION_INDEPENDENT_CODE ON)
# install-exec-hook (KDEINIT_SETUID: Linux, protection against the OOM
# killer)
if(KDEINIT_OOM_PROTECT)
  kde3_install_setuid_root(start_kdeinit)
endif()

kde3_add_executable(start_kdeinit_wrapper
  SOURCES
    start_kdeinit_wrapper.c
)

kde3_add_kdeinit_executable(klauncher
  SOURCES
    klauncher.cpp
    klauncher_main.cpp
    autostart.cpp
  MOC
    klauncher.h
  LINK
    kio
)
