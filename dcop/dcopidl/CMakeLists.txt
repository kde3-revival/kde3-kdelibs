kde3_add_executable(dcopidl
  EXPORT KDE3Targets
  SOURCES
    main.cpp
    yacc.cc
    scanner.cc
  LINK
    KDE3Qt::Qt
)
target_compile_definitions(dcopidl PRIVATE YYDEBUG)
add_executable(KDE3::dcopidl ALIAS dcopidl)
