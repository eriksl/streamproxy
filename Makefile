TARGET			= x86_64
TARGET			= mipsel
DEBUG			= off
DESTDIR			= /usr/local
PROGRAM			= streamproxy
OBJS			= url.o vlog.o queue.o service.o \
				  streamproxy.o acceptsocket.o clientsocket.o \
				  livestreaming.o \
				  filestreaming.o filetranscoding.o \
				  webifrequest.o \
				  demuxer.o encoder.o \
				  mpegts_sr.o mpegts_pat.o mpegts_pmt.o

LDFLAGS			+= -lpthread -lboost_program_options

include common.mak
