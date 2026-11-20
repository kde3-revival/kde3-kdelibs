kde3_add_executable(dcopidl2cpp
  EXPORT KDE3LibsTargets
  SOURCES
    main.cpp
    skel.cpp
    stub.cpp
    stubimpl.cpp
  LINK
    KDE3Qt::Qt
)
add_executable(KDE3::dcopidl2cpp ALIAS dcopidl2cpp)
