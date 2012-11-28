INCLUDES = -DG_LOG_DOMAIN=\"MSU\"

AM_CFLAGS =	$(GLIB_CFLAGS)				\
		$(GIO_CFLAGS)				\
		$(GSSDP_CFLAGS)				\
		$(GUPNP_CFLAGS)				\
		$(GUPNPAV_CFLAGS)			\
		$(GUPNPDLNA_CFLAGS)			\
		$(SOUP_CFLAGS)				\
		-DSYS_CONFIG_DIR="\"$(sysconfdir)\""	\
		-include config.h

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

sysconf_DATA = media-service-upnp.conf

media_service_upnp_sources = 	src/async.c		 \
				src/chain-task.c	 \
				src/device.c		 \
				src/error.c		 \
				src/log.c		 \
				src/media-service-upnp.c \
				src/path.c		 \
				src/props.c		 \
				src/search.c		 \
				src/settings.c		 \
				src/sort.c		 \
				src/task.c		 \
				src/task-processor.c	 \
				src/upnp.c

media_service_upnp_headers =	src/async.h		 \
				src/chain-task.h	 \
				src/client.h		 \
				src/device.h		 \
				src/error.h		 \
				src/interface.h		 \
				src/log.h		 \
				src/media-service-upnp.h \
				src/path.h		 \
				src/props.h		 \
				src/search.h		 \
				src/settings.h		 \
				src/sort.h		 \
				src/task.h		 \
				src/task-atom.h		 \
				src/task-processor.h	 \
				src/upnp.h


bin_PROGRAMS = media-service-upnp

media_service_upnp_SOURCES =	$(media_service_upnp_headers)	\
				$(media_service_upnp_sources)

media_service_upnp_LDADD =	$(GLIB_LIBS)	\
				$(GIO_LIBS)	\
				$(GSSDP_LIBS)	\
				$(GUPNP_LIBS)	\
				$(GUPNPAV_LIBS) \
				$(GUPNPDLNA_LIBS) \
				$(SOUP_LIBS)


dms_info_sources = test/dms-info.c

noinst_PROGRAMS = dms-info
dms_info_SOURCES = $(dms_info_sources)

dms_info_CFLAGS =	$(GLIB_CFLAGS)	\
			$(GIO_CFLAGS)

dms_info_LDADD =	$(GLIB_LIBS)	\
			$(GIO_LIBS)


dbussessiondir = @DBUS_SESSION_DIR@
dbussession_DATA = src/com.intel.media-service-upnp.service

EXTRA_DIST = test/mediaconsole.py	\
	     $(sysconf_DATA)

MAINTAINERCLEANFILES =	Makefile.in		\
			aclocal.m4		\
			configure		\
			config.h.in		\
			config.h.in~		\
			build-aux/depcomp	\
			build-aux/compile	\
			build-aux/missing	\
			build-aux/install-sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = media-service-upnp.pc

CLEANFILES = $(pkgconfig_DATA) $(dbussession_DATA) media-service-upnp.conf
DISTCLEANFILES = $(pkgconfig_DATA) $(dbussession_DATA) media-service-upnp.conf

maintainer-clean-local:
	rm -rf build-aux
