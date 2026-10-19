CXX ?= g++
CC ?= gcc
AR ?= ar
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++20
CFLAGS ?= -Wall -Wextra -O2
LIBS = $(shell pkg-config --libs wayland-client wayland-egl egl gl xkbcommon 2>/dev/null || echo "-lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon")

BUILD_DIR = BUILDS
LIB_TARGET = $(BUILD_DIR)/libwayland_minimums.a
TARGET = full_gl

LIB_OBJS = $(BUILD_DIR)/wayland_window.o $(BUILD_DIR)/shader.o $(BUILD_DIR)/xdg-shell-protocol.o $(BUILD_DIR)/tablet-v2-protocol.o

all: $(BUILD_DIR) $(LIB_TARGET) $(TARGET)

lib: $(BUILD_DIR) $(LIB_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_TARGET): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(TARGET): $(BUILD_DIR)/main.o $(LIB_TARGET)
	$(CXX) $< -L$(BUILD_DIR) -lwayland_minimums $(LIBS) -o $(TARGET)

$(BUILD_DIR)/main.o: main.cpp wayland_window.h shader.h input_mapper.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c main.cpp -o $@

$(BUILD_DIR)/wayland_window.o: wayland_window.cpp wayland_window.h input_mapper.h xdg-shell-client-protocol.h tablet-v2-client-protocol.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c wayland_window.cpp -o $@

$(BUILD_DIR)/shader.o: shader.cpp shader.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c shader.cpp -o $@

$(BUILD_DIR)/xdg-shell-protocol.o: xdg-shell-protocol.c xdg-shell-client-protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c xdg-shell-protocol.c -o $@

$(BUILD_DIR)/tablet-v2-protocol.o: tablet-v2-protocol.c tablet-v2-client-protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c tablet-v2-protocol.c -o $@

clean:
	rm -rf $(BUILD_DIR)/*.o $(LIB_TARGET) $(TARGET)

.PHONY: all lib clean
