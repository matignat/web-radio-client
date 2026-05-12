CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2
LDLIBS = -lssl -lcrypto
TARGET = sikradio

SRCS = arg_parser.cpp \
       http_handler.cpp \
       stream_reciever.cpp \
       sikradio.cpp \
       url_decoder.cpp \
       tls_handler.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS)

%.o: %.cpp common.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild