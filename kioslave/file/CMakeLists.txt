kde3_add_module(kio_file
  SOURCES
    file.cc
  MOC
    file.h
  LINK
    kio
)

install(FILES
  file.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kio)

install(FILES
  file.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
target_compile_definitions(kio_file PRIVATE _LARGEFILE64_SOURCE)
if(USE_POSIX_ACL)
  # libkio.la passed $(ACL_LIBS) on to its users
  target_link_libraries(kio_file PRIVATE ${ACL_LIBS})
endif()
