kde3_add_executable(kconfig_compiler
  EXPORT KDE3Targets
  SOURCES
    kconfig_compiler.cpp
  LINK
    kdecore
)
add_executable(KDE3::kconfig_compiler ALIAS kconfig_compiler)
