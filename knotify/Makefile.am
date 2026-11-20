
INCLUDES= $(all_includes) $(KNOTIFY_PULSE_CFLAGS)

####### Files

kde_module_LTLIBRARIES = knotify.la

# the PulseAudio player, see configure.in.in (HAVE_PULSEAUDIO)
if include_knotify_pulse
KNOTIFY_PULSE_SOURCES = kpulsemainloop.cpp kpulseplayer.cpp
endif

knotify_la_SOURCES	= knotify.cpp knotify.skel $(KNOTIFY_PULSE_SOURCES)
knotify_la_LIBADD	= $(LIB_KDEUI) $(KNOTIFY_PULSE_LIBS)
knotify_la_LDFLAGS = $(all_libraries) -module -avoid-version
knotify_la_METASOURCES = AUTO

noinst_HEADERS	=  knotify.h kpulsemainloop.h kpulseplayer.h

kdelnkdir = $(kde_servicesdir)
kdelnk_DATA = knotify.desktop

messages:
	$(XGETTEXT) knotify.cpp -o $(podir)/knotify.pot

