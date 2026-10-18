CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -Wall -Wextra -O2
CFLAGS ?= -Wall -Wextra -O2
LIBS = $(shell pkg-config --libs wayland-client wayland-egl egl gl 2>/dev/null || echo "-lwayland-client -lwayland-egl -lEGL -lGL")

TARGET = full_gl
OBJS = main.o xdg-shell-protocol.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

main.o: main.cpp xdg-shell-client-protocol.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

xdg-shell-protocol.o: xdg-shell-protocol.c xdg-shell-client-protocol.h
	$(CC) $(CFLAGS) -c xdg-shell-protocol.c -o xdg-shell-protocol.o

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
