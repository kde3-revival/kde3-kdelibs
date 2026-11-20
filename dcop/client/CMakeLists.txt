foreach(_prog dcop dcopfind dcopstart dcopquit)
  kde3_add_executable(${_prog}
    SOURCES
      ${_prog}.cpp
    LINK
      KDE3Qt::Qt
      DCOP
  )
endforeach()

foreach(_prog dcopclient dcopobject dcopref)
  kde3_add_executable(${_prog}
    SOURCES
      ${_prog}.c
  )
endforeach()
