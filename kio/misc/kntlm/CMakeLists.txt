kde3_add_library(kntlm SHARED
  VERSION_INFO 0:0:0
  SOURCES
    kntlm.cpp
    des.cpp
  LINK
    kdecore
)

install(FILES
  kntlm.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kio)
