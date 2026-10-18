CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++11
CFLAGS ?= -Wall -Wextra -O2
LIBS = $(shell pkg-config --libs wayland-client wayland-egl egl gl xkbcommon 2>/dev/null || echo "-lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon")

TARGET = full_gl
OBJS = main.o xdg-shell-protocol.o tablet-v2-protocol.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

main.o: main.cpp xdg-shell-client-protocol.h tablet-v2-client-protocol.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

xdg-shell-protocol.o: xdg-shell-protocol.c xdg-shell-client-protocol.h
	$(CC) $(CFLAGS) -c xdg-shell-protocol.c -o xdg-shell-protocol.o

tablet-v2-protocol.o: tablet-v2-protocol.c tablet-v2-client-protocol.h
	$(CC) $(CFLAGS) -c tablet-v2-protocol.c -o tablet-v2-protocol.o

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
