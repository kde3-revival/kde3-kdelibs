kde3_add_kdeinit_executable(kconf_update
  SOURCES
    kconf_update.cpp
  LINK
    kdecore
)
target_include_directories(kdeinit_kconf_update PRIVATE "${PROJECT_SOURCE_DIR}/kded")
