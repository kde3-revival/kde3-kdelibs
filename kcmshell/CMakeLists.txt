kde3_add_kdeinit_executable(kcmshell
  SOURCES
    main.cpp
    main.skel
  MOC
    main.h
  LINK
    kutils
)
target_include_directories(kdeinit_kcmshell PRIVATE "${PROJECT_SOURCE_DIR}/kutils")
