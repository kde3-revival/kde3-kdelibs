# kballoontest: KBalloon's balloons on each monitor (see kballoontest.cpp)
kde3_add_executable(kballoontest NO_INSTALL
  SOURCES kballoontest.cpp
  MOC kballoontest.h
  LINK kdeui)
