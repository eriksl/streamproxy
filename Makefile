TARGET			= mipsel
TARGET			= x86_64
DEBUG			= off
DESTDIR			= /usr/local
PROGRAM			= streamproxy
OBJS			= url.o vlog.o queue.o service.o \
				  streamproxy.o acceptsocket.o streamingsocket.o \
				  livestreaming.o webifrequest.o \
				  demuxer.o encoder.o \
				  mpegts.o mpegts_sr.o mpegts_pat.o mpegts_pmt.o

LDFLAGS			+= -lpthread -lboost_program_options

include common.mak
