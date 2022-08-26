SRC = audio_decoder.cpp file_reader.cpp frame_buffer.cpp frame.cpp \
	main.cpp packet.cpp player.cpp timer.cpp utils.cpp video_player.cpp \
	webm_player.cpp
	
SRC += mkvparser/mkvparser.cc mkvparser/mkvreader.cc

CXXFLAGS = -g -Wall -I /usr/include/SDL -I . -I mkvparser

player: $(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(SRC)))
	$(CXX) -o $@ $^ -lSDL -lpthread -lvorbis -lvpx
