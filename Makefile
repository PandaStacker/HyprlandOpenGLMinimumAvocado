CXX ?= g++
CC ?= gcc
AR ?= ar
CXXFLAGS ?= -Wall -Wextra -std=c++20 -Iinclude
CFLAGS ?= -Wall -Wextra
LIBS = $(shell pkg-config --libs wayland-client vulkan xkbcommon 2>/dev/null || echo "-lwayland-client -lvulkan -lxkbcommon")

BUILD_DIR = BUILDS
LIB_TARGET = $(BUILD_DIR)/libwayland_minimums.a
TARGET = wayland_app

LIB_OBJS = $(BUILD_DIR)/wayland_window.o $(BUILD_DIR)/shader.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/app.o $(BUILD_DIR)/client.o $(BUILD_DIR)/server.o $(BUILD_DIR)/user_controller.o $(BUILD_DIR)/xdg-shell-protocol.o $(BUILD_DIR)/tablet-v2-protocol.o

all: $(BUILD_DIR) $(LIB_TARGET) $(TARGET)

lib: $(BUILD_DIR) $(LIB_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_TARGET): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(TARGET): $(BUILD_DIR)/main.o $(LIB_TARGET)
	$(CXX) $< -L$(BUILD_DIR) -lwayland_minimums $(LIBS) -o $(TARGET)

$(BUILD_DIR)/main.o: main.cpp app.hpp client.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c main.cpp -o $@

$(BUILD_DIR)/wayland_window.o: wayland_window.cpp wayland_window.hpp input_mapper.hpp protocols/xdg-shell-client-protocol.h protocols/tablet-v2-client-protocol.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c wayland_window.cpp -o $@

$(BUILD_DIR)/shader.o: shader.cpp shader.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c shader.cpp -o $@

$(BUILD_DIR)/graphics.o: graphics.cpp graphics.hpp shader.hpp client.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c graphics.cpp -o $@

$(BUILD_DIR)/app.o: app.cpp app.hpp wayland_window.hpp graphics.hpp client.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c app.cpp -o $@

$(BUILD_DIR)/client.o: client.cpp client.hpp input_mapper.hpp user_controller.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c client.cpp -o $@

$(BUILD_DIR)/server.o: server.cpp server.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c server.cpp -o $@

$(BUILD_DIR)/user_controller.o: user_controller.cpp user_controller.hpp input_mapper.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c user_controller.cpp -o $@

$(BUILD_DIR)/xdg-shell-protocol.o: protocols/xdg-shell-protocol.c protocols/xdg-shell-client-protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c protocols/xdg-shell-protocol.c -o $@

$(BUILD_DIR)/tablet-v2-protocol.o: protocols/tablet-v2-protocol.c protocols/tablet-v2-client-protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c protocols/tablet-v2-protocol.c -o $@

clean:
	rm -rf $(BUILD_DIR)/*.o $(LIB_TARGET) $(TARGET)

.PHONY: all lib clean
