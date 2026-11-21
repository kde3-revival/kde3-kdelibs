kde3_add_kdeinit_executable(cupsdconf
  SOURCES
    cupsdconf.cpp
    cupsddialog.cpp
    cupsdpage.cpp
    cupsdcomment.cpp
    cupsdsplash.cpp
    cupsdserverpage.cpp
    cupsdlogpage.cpp
    cupsdjobspage.cpp
    cupsdfilterpage.cpp
    qdirlineedit.cpp
    cupsddirpage.cpp
    portdialog.cpp
    cupsdnetworkpage.cpp
    editlist.cpp
    cupsdbrowsingpage.cpp
    browsedialog.cpp
    cupsdsecuritypage.cpp
    locationdialog.cpp
    addressdialog.cpp
    cups-util.c
    qdirmultilineedit.cpp
    sizewidget.cpp
    main.cpp
  MOC
    browsedialog.h
    cupsdbrowsingpage.h
    cupsddialog.h
    cupsdjobspage.h
    cupsdnetworkpage.h
    cupsdpage.h
    cupsdsecuritypage.h
    cupsdserverpage.h
    editlist.h
    locationdialog.h
    qdirlineedit.h
    qdirmultilineedit.h
  LINK
    kio
    Cups::Cups
)

# The directories CUPS was built with (cups.pc); the CUPS defaults
# without it.  ServerBin is <prefix>/lib/cups even on systems whose
# libraries are in lib64.
pkg_get_variable(CUPS_SERVERBIN cups cups_serverbin)
pkg_get_variable(CUPS_SERVERROOT cups cups_serverroot)
pkg_get_variable(CUPS_DATADIR cups cups_datadir)
foreach(_v SERVERBIN:/usr/lib/cups SERVERROOT:/etc/cups DATADIR:/usr/share/cups)
  string(REPLACE ":" ";" _v "${_v}")
  list(GET _v 0 _n)
  list(GET _v 1 _d)
  if(NOT CUPS_${_n})
    set(CUPS_${_n} "${_d}")
  endif()
endforeach()
target_compile_definitions(kdeinit_cupsdconf PRIVATE
  "KDE3_CUPS_SERVERBIN=\"${CUPS_SERVERBIN}\""
  "KDE3_CUPS_SERVERROOT=\"${CUPS_SERVERROOT}\""
  "KDE3_CUPS_DATADIR=\"${CUPS_DATADIR}\"")

install(FILES
  kde_logo.png
  cups_logo.png
  cupsd.conf.template
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kdeprint)
