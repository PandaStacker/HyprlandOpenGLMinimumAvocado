CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++11
CFLAGS ?= -Wall -Wextra -O2
LIBS = $(shell pkg-config --libs wayland-client wayland-egl egl gl xkbcommon 2>/dev/null || echo "-lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon")

BUILD_DIR = BUILDS
TARGET = full_gl
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/xdg-shell-protocol.o $(BUILD_DIR)/tablet-v2-protocol.o

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

$(BUILD_DIR)/main.o: main.cpp xdg-shell-client-protocol.h tablet-v2-client-protocol.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c main.cpp -o $@

$(BUILD_DIR)/xdg-shell-protocol.o: xdg-shell-protocol.c xdg-shell-client-protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c xdg-shell-protocol.c -o $@

$(BUILD_DIR)/tablet-v2-protocol.o: tablet-v2-protocol.c tablet-v2-client-protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c tablet-v2-protocol.c -o $@

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

.PHONY: all clean
