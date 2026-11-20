AM_CPPFLAGS = -I$(top_srcdir)/kdecore -I$(top_srcdir) $(all_includes)

bin_PROGRAMS = kconfig_compiler

kconfig_compiler_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kconfig_compiler_LDADD = $(LIB_KDECORE)
kconfig_compiler_SOURCES = kconfig_compiler.cpp
