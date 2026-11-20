# libvcards.la (convenience library of libkabc)
kde3_add_library(vcards OBJECT
  SOURCES
    vcard.cpp
    vcardline.cpp
    vcardparser.cpp
  LINK
    kdecore
)
add_dependencies(vcards kabc_addressee)

install(FILES
  vcard.h
  vcardline.h
  vcardparser.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kabc)
