INCLUDES = \
	-I$(srcdir)/include \
	-I$(srcdir)/include/generated \
	$(all_includes)

### KDE 4.0: either make noinst or rename to something like libkvcard
lib_LTLIBRARIES = libvcard.la

libvcard_la_SOURCES = vCard-all.cpp
libvcard_la_LDFLAGS = $(all_libraries)
libvcard_la_LIBADD = $(LIB_KDECORE)
