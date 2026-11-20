# ksvgtopng needs svgicons (libart_lgpl), which is not built
# (include_SVGICONS).

add_subdirectory(hicolor)
add_subdirectory(crystalsvg)
add_subdirectory(emoticons)

# install-data-local: default.kde -> crystalsvg
kde3_install_symlink(crystalsvg ${KDE3_ICON_INSTALL_DIR}/default.kde)
