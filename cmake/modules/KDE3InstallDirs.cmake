#[=======================================================================[.rst:
KDE3InstallDirs
---------------

The KDE 3 installation layout (the kde_*dir / xdg_*dir variables of
autotools' KDE_SET_DEFAULT_PATHS).  Every ``KDE3_*_INSTALL_DIR`` is a
cache variable relative to ``CMAKE_INSTALL_PREFIX`` (an absolute value
is accepted as well); ``KDE3_FULL_*_INSTALL_DIR`` holds the absolute
path for compiled-in paths.

=============================  ======================  ===================
Variable                       Default                 autotools
=============================  ======================  ===================
KDE3_BIN_INSTALL_DIR           bin                     bindir, kde_bindir
KDE3_LIB_INSTALL_DIR           lib                     libdir
KDE3_INCLUDE_INSTALL_DIR       include                 includedir
KDE3_MODULE_INSTALL_DIR        lib/kde3                kde_moduledir
KDE3_STYLE_INSTALL_DIR         lib/kde3/plugins/styles kde_styledir
KDE3_WIDGET_INSTALL_DIR        lib/kde3/plugins/designer kde_widgetdir
KDE3_DATA_ROOT_INSTALL_DIR     share                   datadir
KDE3_APPS_INSTALL_DIR          share/applnk            kde_appsdir
KDE3_DATA_INSTALL_DIR          share/apps              kde_datadir
KDE3_CONFIG_INSTALL_DIR        share/config            kde_confdir
KDE3_KCFG_INSTALL_DIR          share/config.kcfg       kde_kcfgdir
KDE3_HTML_INSTALL_DIR          share/doc/HTML          kde_htmldir
KDE3_ICON_INSTALL_DIR          share/icons             kde_icondir
KDE3_LOCALE_INSTALL_DIR        share/locale            kde_locale
KDE3_MIME_INSTALL_DIR          share/mimelnk           kde_mimedir
KDE3_SERVICES_INSTALL_DIR      share/services          kde_servicesdir
KDE3_SERVICETYPES_INSTALL_DIR  share/servicetypes      kde_servicetypesdir
KDE3_SOUND_INSTALL_DIR         share/sounds            kde_sounddir
KDE3_TEMPLATES_INSTALL_DIR     share/templates         kde_templatesdir
KDE3_WALLPAPER_INSTALL_DIR     share/wallpapers        kde_wallpaperdir
KDE3_AUTOSTART_INSTALL_DIR     share/autostart         (resource autostart)
KDE3_SYSCONF_INSTALL_DIR       etc                     sysconfdir
KDE3_XDG_APPS_INSTALL_DIR      share/applications/kde  xdg_appsdir
KDE3_XDG_MENU_INSTALL_DIR      etc/xdg/menus           xdg_menudir
KDE3_XDG_DIRECTORY_INSTALL_DIR share/desktop-directories xdg_directorydir
KDE3_CMAKE_INSTALL_DIR         lib/cmake/KDE3          (new)
=============================  ======================  ===================
#]=======================================================================]

function(_kde3_install_dir var default doc)
  # Like GNUInstallDirs: set(... CACHE PATH) would turn a relative
  # -DKDE3_<var>_INSTALL_DIR=<dir> given without a type into an absolute
  # path below the build directory, so only give such a value its type.
  if(NOT DEFINED KDE3_${var}_INSTALL_DIR)
    set(KDE3_${var}_INSTALL_DIR "${default}" CACHE PATH "${doc}")
  else()
    get_property(_type CACHE KDE3_${var}_INSTALL_DIR PROPERTY TYPE)
    if(_type STREQUAL "UNINITIALIZED")
      set_property(CACHE KDE3_${var}_INSTALL_DIR PROPERTY TYPE PATH)
      set_property(CACHE KDE3_${var}_INSTALL_DIR PROPERTY HELPSTRING "${doc}")
    endif()
  endif()
  set(_dir "${KDE3_${var}_INSTALL_DIR}")
  if(IS_ABSOLUTE "${_dir}")
    set(KDE3_FULL_${var}_INSTALL_DIR "${_dir}" PARENT_SCOPE)
  else()
    set(KDE3_FULL_${var}_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${_dir}" PARENT_SCOPE)
  endif()
endfunction()

_kde3_install_dir(BIN            bin                        "Executables (bindir)")
_kde3_install_dir(LIB            lib                        "Libraries (libdir)")
_kde3_install_dir(INCLUDE        include                    "Headers (includedir)")
_kde3_install_dir(MODULE         lib/kde3                   "Plugins and kdeinit modules (kde_moduledir)")
_kde3_install_dir(STYLE          lib/kde3/plugins/styles    "Qt style plugins (kde_styledir)")
_kde3_install_dir(WIDGET         lib/kde3/plugins/designer  "Qt Designer plugins (kde_widgetdir)")
_kde3_install_dir(DATA_ROOT      share                      "Architecture independent data (datadir)")
_kde3_install_dir(APPS           share/applnk               "Application .desktop files (kde_appsdir)")
_kde3_install_dir(DATA           share/apps                 "Application data (kde_datadir)")
_kde3_install_dir(CONFIG         share/config               "Default configuration (kde_confdir)")
_kde3_install_dir(KCFG           share/config.kcfg          "KConfigXT .kcfg files (kde_kcfgdir)")
_kde3_install_dir(HTML           share/doc/HTML             "Handbooks (kde_htmldir)")
_kde3_install_dir(ICON           share/icons                "Icons (kde_icondir)")
_kde3_install_dir(LOCALE         share/locale               "Translations (kde_locale)")
_kde3_install_dir(MIME           share/mimelnk              "Mime type .desktop files (kde_mimedir)")
_kde3_install_dir(SERVICES       share/services             "Service .desktop files (kde_servicesdir)")
_kde3_install_dir(SERVICETYPES   share/servicetypes         "Service types (kde_servicetypesdir)")
_kde3_install_dir(SOUND          share/sounds               "Sounds (kde_sounddir)")
_kde3_install_dir(TEMPLATES      share/templates            "\"Create New\" templates (kde_templatesdir)")
_kde3_install_dir(WALLPAPER      share/wallpapers           "Wallpapers (kde_wallpaperdir)")
_kde3_install_dir(AUTOSTART      share/autostart            "Autostart .desktop files")
_kde3_install_dir(SYSCONF        etc                        "System configuration (sysconfdir)")
_kde3_install_dir(XDG_APPS       share/applications/kde     "XDG application .desktop files (xdg_appsdir)")
_kde3_install_dir(XDG_MENU       etc/xdg/menus              "XDG menus (xdg_menudir)")
_kde3_install_dir(XDG_DIRECTORY  share/desktop-directories  "XDG menu directories (xdg_directorydir)")
_kde3_install_dir(CMAKE          lib/cmake/KDE3             "CMake package configuration")
