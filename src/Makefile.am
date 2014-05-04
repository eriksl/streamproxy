bin_PROGRAMS = streamproxy

streamproxy_SOURCES = \
	acceptsocket.cpp \
	clientsocket.cpp \
	configmap.cpp \
	demuxer.cpp \
	enigma_settings.cpp \
	encoder.cpp \
	filestreaming.cpp \
	filetranscoding.cpp \
	livestreaming.cpp \
	livetranscoding.cpp \
	mpegts.cpp \
	queue.cpp \
	service.cpp \
	streamproxy.cpp \
	time_offset.cpp \
	url.cpp \
	util.cpp \
	webifrequest.cpp

streamproxy_CXXFLAGS = -Wall -Wextra -Wshadow -Wundef -Wformat=2 -Winit-self -Wunused -Werror -Wpointer-arith -Wcast-qual -Wmultichar
streamproxy_LDADD = -lpthread -lboost_program_options -lcrypt
