# kdemacros.h and kdelibs_export.h are used by every library, including
# kdefx and DCOP's users that do not link kdecore.
configure_file(kdemacros.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/kdemacros.h")
add_library(kdemacros INTERFACE)
target_include_directories(kdemacros INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR}>"
  "$<INSTALL_INTERFACE:${KDE3_INCLUDE_INSTALL_DIR}>")
install(TARGETS kdemacros EXPORT KDE3Targets)
add_library(KDE3::kdemacros ALIAS kdemacros)

add_subdirectory(network)
# svgicons needs libart_lgpl (HAVE_LIBART), which is not available.

set(_kdecore_idn)
if(HAVE_LIBIDN)
  set(_kdecore_idn PkgConfig::LIBIDN)
endif()

kde3_add_library(kdecore SHARED
  VERSION_INFO 6:0:2
  SOURCES
    libintl.cpp
    kapplication.cpp
    kdebug.cpp
    netwm.cpp
    kconfigbase.cpp
    kconfig.cpp
    ksimpleconfig.cpp
    kconfigbackend.cpp
    kmanagerselection.cpp
    kdesktopfile.cpp
    kstandarddirs.cpp
    ksock.cpp
    kpty.cpp
    kprocess.cpp
    kprocctrl.cpp
    klocale.cpp
    krfcdate.cpp
    kiconeffect.cpp
    kicontheme.cpp
    kiconloader.cpp
    kwin.cpp
    kwinmodule.cpp
    krootprop.cpp
    kcharsets.cpp
    kckey.cpp
    kshortcut.cpp
    kkeynative_x11.cpp
    kkeyserver_x11.cpp
    kaccelaction.cpp
    kshortcutmenu.cpp
    kaccelbase.cpp
    kaccel.cpp
    kglobalaccel_x11.cpp
    kglobalaccel.cpp
    kstdaccel.cpp
    kshortcutlist.cpp
    kcrash.cpp
    kurl.cpp
    kregexp.cpp
    kglobal.cpp
    kglobalsettings.cpp
    kallocator.cpp
    kvmallocator.cpp
    kmimesourcefactory.cpp
    kinstance.cpp
    kpalette.cpp
    kipc.cpp
    klibloader.cpp
    ktempfile.cpp
    kuniqueapplication.cpp
    kaccelmanager.cpp
    ksavefile.cpp
    krandomsequence.cpp
    kstringhandler.cpp
    kcompletion.cpp
    kcmdlineargs.cpp
    kaboutdata.cpp
    kcompletionbase.cpp
    knotifyclient.cpp
    kaudioplayer.cpp
    kdcoppropertyproxy.cpp
    ksockaddr.cpp
    kextsock.cpp
    netsupp.cpp
    kprocio.cpp
    kbufferedio.cpp
    kpixmapprovider.cpp
    kurldrag.cpp
    kmdcodec.cpp
    ksocks.cpp
    ksycoca.cpp
    ksycocadict.cpp
    ksycocafactory.cpp
    ksycoca.skel
    kxmessages.cpp
    kstartupinfo.cpp
    kcatalogue.cpp
    kasyncio.cpp
    kmultipledrag.cpp
    kstaticdeleter.cpp
    kappdcopiface.cpp
    kappdcopiface.skel
    kclipboard.cpp
    kcheckaccelerators.cpp
    kdeversion.cpp
    kdebugdcopiface.cpp
    kdebugdcopiface.skel
    kcalendarsystem.cpp
    kcalendarsystemgregorian.cpp
    kcalendarsystemhijri.cpp
    kcalendarsystemhebrew.cpp
    kcalendarsystemfactory.cpp
    kmacroexpander.cpp
    kidna.cpp
    ktempdir.cpp
    kshell.cpp
    kmountpoint.cpp
    kcalendarsystemjalali.cpp
    kprotocolinfo_kdecore.cpp
    kprotocolinfofactory.cpp
    kxerrorhandler.cpp
    kuser.cpp
    kconfigskeleton.cpp
    kconfigdialogmanager.cpp
    klockfile.cpp
    kqiodevicegzip_p.cpp
    ktimezones.cpp
    kdlopen.cpp
  MOC
    kaccel.h
    kaccelmanager_private.h
    kaccelprivate.h
    kapplication.h
    kasyncio.h
    kaudioplayer.h
    kbufferedio.h
    kcheckaccelerators.h
    kclipboard.h
    kcompletion.h
    kconfig.h
    kconfigbase.h
    kconfigdialogmanager.h
    kdesktopfile.h
    kextsock.h
    kglobalaccel.h
    kglobalaccel_x11.h
    kiconloader_p.h
    klibloader.h
    kmanagerselection.h
    kmultipledrag.h
    kprocctrl.h
    kprocess.h
    kprocio.h
    kshortcutmenu.h
    ksimpleconfig.h
    ksock.h
    ksockaddr.h
    kstartupinfo.h
    ksycoca.h
    kuniqueapplication.h
    kwinmodule.h
    kxmessages.h
  LINK
    DCOP
    kdefx
    kdemacros
  LINK_PRIVATE
    kdecorenetwork
    X11::Xext
    ${KDE3_LIBUTIL}
    ${KDE3_LIBRESOLV}
    ${_kdecore_idn}
)

# all_includes: -I$(top_builddir)/kdecore -I$(top_srcdir)/kdecore
# -I$(top_srcdir)/kdecore/network
target_include_directories(kdecore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR};${CMAKE_CURRENT_SOURCE_DIR}/network>")

# kde-config.cpp from kde-config.cpp.in: the paths as autoconf wrote them
# (${prefix}/...), expanded by "kde-config --expandvars".
function(_kde_config_dir var dir)
  if(IS_ABSOLUTE "${dir}")
    set(${var} "${dir}" PARENT_SCOPE)
  else()
    set(${var} "\${prefix}/${dir}" PARENT_SCOPE)
  endif()
endfunction()
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
_kde_config_dir(datadir "${KDE3_DATA_ROOT_INSTALL_DIR}")
_kde_config_dir(libdir "${KDE3_LIB_INSTALL_DIR}")
_kde_config_dir(includedir "${KDE3_INCLUDE_INSTALL_DIR}")
_kde_config_dir(sysconfdir "${KDE3_SYSCONF_INSTALL_DIR}")
_kde_config_dir(kde_appsdir "${KDE3_APPS_INSTALL_DIR}")
_kde_config_dir(kde_bindir "${KDE3_BIN_INSTALL_DIR}")
_kde_config_dir(kde_confdir "${KDE3_CONFIG_INSTALL_DIR}")
_kde_config_dir(kde_datadir "${KDE3_DATA_INSTALL_DIR}")
_kde_config_dir(kde_htmldir "${KDE3_HTML_INSTALL_DIR}")
_kde_config_dir(kde_icondir "${KDE3_ICON_INSTALL_DIR}")
_kde_config_dir(kde_kcfgdir "${KDE3_KCFG_INSTALL_DIR}")
_kde_config_dir(kde_locale "${KDE3_LOCALE_INSTALL_DIR}")
_kde_config_dir(kde_mimedir "${KDE3_MIME_INSTALL_DIR}")
_kde_config_dir(kde_moduledir "${KDE3_MODULE_INSTALL_DIR}")
_kde_config_dir(kde_servicesdir "${KDE3_SERVICES_INSTALL_DIR}")
_kde_config_dir(kde_servicetypesdir "${KDE3_SERVICETYPES_INSTALL_DIR}")
_kde_config_dir(kde_sounddir "${KDE3_SOUND_INSTALL_DIR}")
_kde_config_dir(kde_templatesdir "${KDE3_TEMPLATES_INSTALL_DIR}")
_kde_config_dir(kde_wallpaperdir "${KDE3_WALLPAPER_INSTALL_DIR}")
_kde_config_dir(xdg_appsdir "${KDE3_XDG_APPS_INSTALL_DIR}")
_kde_config_dir(xdg_directorydir "${KDE3_XDG_DIRECTORY_INSTALL_DIR}")
_kde_config_dir(xdg_menudir "${KDE3_XDG_MENU_INSTALL_DIR}")
configure_file(kde-config.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/kde-config.cpp" @ONLY)

kde3_add_executable(kde-config
  SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/kde-config.cpp
  LINK
    kdecore
)

# install-exec-hook
kde3_add_executable(kgrantpty
  SOURCES
    kgrantpty.c
)
set_target_properties(kgrantpty PROPERTIES POSITION_INDEPENDENT_CODE ON)
kde3_install_setuid_root(kgrantpty)

install(FILES
  kconfig.h
  kconfigskeleton.h
  kconfigdata.h
  ksimpleconfig.h
  kconfigdialogmanager.h
  kconfigbase.h
  kdesktopfile.h
  kurl.h
  ksock.h
  kaboutdata.h
  kcmdlineargs.h
  kconfigbackend.h
  kapp.h
  kapplication.h
  kuniqueapp.h
  kuniqueapplication.h
  kcharsets.h
  kdeversion.h
  kpty.h
  kprocess.h
  kprocctrl.h
  klocale.h
  kicontheme.h
  kiconloader.h
  kdebug.h
  kwinmodule.h
  kwin.h
  krootprop.h
  kshortcut.h
  kkeynative.h
  kaccel.h
  kglobalaccel.h
  kstdaccel.h
  kshortcutlist.h
  kcatalogue.h
  kregexp.h
  kcompletion.h
  kstringhandler.h
  kstddirs.h
  kstandarddirs.h
  kglobal.h
  kglobalsettings.h
  ksharedptr.h
  kallocator.h
  kvmallocator.h
  kcrash.h
  krfcdate.h
  kinstance.h
  kpalette.h
  kipc.h
  klibloader.h
  ktempfile.h
  ksavefile.h
  krandomsequence.h
  knotifyclient.h
  kiconeffect.h
  kaudioplayer.h
  kdcoppropertyproxy.h
  netwm.h
  kaccelmanager.h
  netwm_def.h
  kpixmapprovider.h
  kunload.h
  kstaticdeleter.h
  kextsock.h
  kextendedsocket.h
  ksockaddr.h
  kprocio.h
  kasyncio.h
  kbufferedio.h
  kurldrag.h
  kmimesourcefactory.h
  kmdcodec.h
  ksocks.h
  ksycoca.h
  ksycocaentry.h
  ksycocatype.h
  kxmessages.h
  kstartupinfo.h
  klargefile.h
  kmultipledrag.h
  kgenericfactory.h
  kgenericfactory.tcc
  ktypelist.h
  ksortablevaluelist.h
  kdebugclasses.h
  kclipboard.h
  kcalendarsystem.h
  kcalendarsystemfactory.h
  kmacroexpander.h
  kmanagerselection.h
  kmountpoint.h
  kuser.h
  klockfile.h
  kidna.h
  ktempdir.h
  kshell.h
  fixx11h.h
  kxerrorhandler.h
  kdelibs_export.h
  ${CMAKE_CURRENT_BINARY_DIR}/kdemacros.h
  kde_file.h
  ktimezones.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

# install-data-local
install(FILES kmdcodec_compat.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kio RENAME kmdcodec.h)
install(FILES all_languages.desktop
  DESTINATION ${KDE3_LOCALE_INSTALL_DIR} RENAME all_languages)
install(FILES eventsrc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/knotify)

install(FILES
  kdebug.areas
  kdebugrc
  language.codes
  DESTINATION ${KDE3_CONFIG_INSTALL_DIR})

# If you add a color palette file here, please change
# kdelibs/kdeui/kcolordialog.cpp too to allow to translate the name
install(FILES
  40.colors
  Web.colors
  Royal.colors
  Rainbow.colors
  DESTINATION ${KDE3_CONFIG_INSTALL_DIR}/colors)

add_subdirectory(kconfig_compiler)
