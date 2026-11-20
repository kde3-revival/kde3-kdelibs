
INCLUDES= $(all_includes)

####### Files

kde_module_LTLIBRARIES = knotify.la

knotify_la_SOURCES	= knotify.cpp knotify.skel
knotify_la_LIBADD	= $(LIB_KDEUI)
knotify_la_LDFLAGS = $(all_libraries) -module -avoid-version
knotify_la_METASOURCES = AUTO

noinst_HEADERS	=  knotify.h

kdelnkdir = $(kde_servicesdir)
kdelnk_DATA = knotify.desktop

messages:
	$(XGETTEXT) knotify.cpp -o $(podir)/knotify.pot

