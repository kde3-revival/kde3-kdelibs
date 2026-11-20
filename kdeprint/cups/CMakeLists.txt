# INCLUDES
include_directories("${PROJECT_SOURCE_DIR}/kdeprint" "${PROJECT_SOURCE_DIR}/kdeprint/management")

kde3_add_module(kdeprint_cups
  SOURCES
    cupsinfos.cpp
    ipprequest.cpp
    kmcupsfactory.cpp
    kmcupsjobmanager.cpp
    kmcupsmanager.cpp
    kmcupsuimanager.cpp
    kmpropbanners.cpp
    kmwbanners.cpp
    kmwipp.cpp
    kmwippselect.cpp
    kcupsprinterimpl.cpp
    kmcupsconfigwidget.cpp
    kmconfigcups.cpp
    kmconfigcupsdir.cpp
    kmwippprinter.cpp
    kmwfax.cpp
    kmwquota.cpp
    kmpropquota.cpp
    kmwusers.cpp
    kmpropusers.cpp
    kpschedulepage.cpp
    image.cpp
    imagepreview.cpp
    imageposition.cpp
    kpimagepage.cpp
    kptextpage.cpp
    kphpgl2page.cpp
    cupsaddsmb2.cpp
    kmwother.cpp
    ippreportdlg.cpp
    kptagspage.cpp
  MOC
    cupsaddsmb2.h
    ippreportdlg.h
    kmcupsjobmanager.h
    kmcupsmanager.h
    kmwippprinter.h
    kmwother.h
    kmwquota.h
    kmwusers.h
    kpimagepage.h
    kpschedulepage.h
    kptextpage.h
  LINK
    kdeprint_management
    kio
    Cups::Cups
)

kde3_add_executable(make_driver_db_cups
  SOURCES
    make_driver_db_cups.c
  LINK
    driverparse
    ${CMAKE_DL_LIBS}
    ZLIB::ZLIB
)

kde3_add_executable(cupsdoprint
  SOURCES
    cupsdoprint.c
  LINK
    Cups::Cups
)

install(FILES
  cups.print
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kdeprint/plugins)

install(FILES
  preview.png
  preview-mini.png
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kdeprint)

# install-data-local: the PostScript driver for CUPS, into CUPS' model
# directory if it exists (outside the KDE prefix, failures ignored)
option(KDE3_INSTALL_CUPS_PPD "Install postscript.ppd.gz into CUPS' model directory" ON)
if(KDE3_INSTALL_CUPS_PPD)
  foreach(_d /usr/share/cups /usr/local/share/cups /opt/share/cups /opt/local/share/cups)
    if(IS_DIRECTORY "${_d}/model")
      # "$(INSTALL_DATA) ... || true": a user installing into his own
      # prefix without DESTDIR cannot write there; note it and go on.
      set(_ppd "${CMAKE_CURRENT_SOURCE_DIR}/postscript.ppd.gz")
      install(CODE "
        set(_dir \"\$ENV{DESTDIR}${_d}/model\")
        execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E make_directory \"\${_dir}\"
          RESULT_VARIABLE _r OUTPUT_QUIET ERROR_QUIET)
        if(_r EQUAL 0)
          execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E copy \"${_ppd}\" \"\${_dir}/\"
            RESULT_VARIABLE _r OUTPUT_QUIET ERROR_QUIET)
        endif()
        if(_r EQUAL 0)
          message(STATUS \"Installing: \${_dir}/postscript.ppd.gz\")
          list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"\${_dir}/postscript.ppd.gz\")
        else()
          message(STATUS \"Not installed (no permission): \${_dir}/postscript.ppd.gz\")
        endif()")
      break()
    endif()
  endforeach()
endif()

add_subdirectory(cupsdconf2)

