bin_PROGRAMS = streamproxy

streamproxy_SOURCES = \
	acceptsocket.cpp \
	clientsocket.cpp \
	demuxer.cpp \
	encoder.cpp \
	filestreaming.cpp \
	filetranscoding.cpp \
	livestreaming.cpp \
	livetranscoding.cpp \
	mpegts_pat.cpp \
	mpegts_pmt.cpp \
	mpegts_sr.cpp \
	queue.cpp \
	service.cpp \
	streamproxy.cpp \
	url.cpp \
	vlog.cpp \
	webifrequest.cpp

streamproxy_CXXFLAGS = -Wall -Wextra -Wshadow -Wundef -Wformat=2 -Winit-self -Wunused -Werror -Wpointer-arith -Wcast-qual -Wmultichar
streamproxy_LDADD = -lpthread -lboost_program_options
