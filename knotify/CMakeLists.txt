set(_pulse_sources)
set(_pulse_moc)
if(HAVE_PULSEAUDIO)
  set(_pulse_sources kpulsemainloop.cpp kpulseplayer.cpp)
  set(_pulse_moc kpulsemainloop.h kpulseplayer.h)
endif()

kde3_add_module(knotify
  SOURCES
    knotify.cpp
    knotify.skel
    ${_pulse_sources}
  MOC
    knotify.h
    ${_pulse_moc}
  LINK
    kdeui
)
if(HAVE_PULSEAUDIO)
  target_link_libraries(knotify PRIVATE PkgConfig::KNOTIFY_PULSE)
endif()

install(FILES
  knotify.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
