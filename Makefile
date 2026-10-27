CXX ?= g++
CC ?= gcc
AR ?= ar
CXXFLAGS ?= -Wall -Wextra -std=c++20 -Iinclude
CFLAGS ?= -Wall -Wextra
CXXFLAGS += -MMD -MP
CFLAGS += -MMD -MP
LDFLAGS ?= -fuse-ld=gold
LIBS = $(shell pkg-config --libs wayland-client vulkan xkbcommon 2>/dev/null || echo "-lwayland-client -lvulkan -lxkbcommon")

BUILD_DIR = BUILDS
LIB_TARGET = $(BUILD_DIR)/libwayland_minimums.a
TARGET = wayland_app

SHADERS = shaders/triangle.vert.spv shaders/triangle.frag.spv
PCH_SRC = include/pch.hpp
PCH_GCH = $(BUILD_DIR)/pch.hpp.gch
PCH_FLAGS = -I$(BUILD_DIR) -include pch.hpp

LIB_OBJS = $(BUILD_DIR)/wayland_window.o $(BUILD_DIR)/shader.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/app.o $(BUILD_DIR)/client.o $(BUILD_DIR)/server.o $(BUILD_DIR)/user_controller.o $(BUILD_DIR)/xdg-shell-protocol.o $(BUILD_DIR)/tablet-v2-protocol.o $(BUILD_DIR)/viewporter-protocol.o $(BUILD_DIR)/fractional-scale-v1-protocol.o $(BUILD_DIR)/tearing-control-v1-protocol.o

all: $(BUILD_DIR) $(PCH_GCH) $(SHADERS) $(LIB_TARGET) $(TARGET)

lib: $(BUILD_DIR) $(PCH_GCH) $(LIB_TARGET)

shaders: $(SHADERS)

pch: $(PCH_GCH)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(PCH_GCH): $(PCH_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -x c++-header $(PCH_SRC) -o $@

shaders/%.spv: shaders/%
	glslc $< -o $@

$(LIB_TARGET): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(TARGET): $(BUILD_DIR)/main.o $(LIB_TARGET)
	$(CXX) $< -L$(BUILD_DIR) -lwayland_minimums $(LIBS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/main.o: main.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c main.cpp -o $@

$(BUILD_DIR)/wayland_window.o: wayland_window.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c wayland_window.cpp -o $@

$(BUILD_DIR)/shader.o: shader.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c shader.cpp -o $@

$(BUILD_DIR)/graphics.o: graphics.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c graphics.cpp -o $@

$(BUILD_DIR)/app.o: app.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c app.cpp -o $@

$(BUILD_DIR)/client.o: client.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c client.cpp -o $@

$(BUILD_DIR)/server.o: server.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c server.cpp -o $@

$(BUILD_DIR)/user_controller.o: user_controller.cpp $(PCH_GCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PCH_FLAGS) -c user_controller.cpp -o $@

$(BUILD_DIR)/xdg-shell-protocol.o: protocols/xdg-shell-protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c protocols/xdg-shell-protocol.c -o $@

$(BUILD_DIR)/tablet-v2-protocol.o: protocols/tablet-v2-protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c protocols/tablet-v2-protocol.c -o $@

$(BUILD_DIR)/viewporter-protocol.o: protocols/viewporter-protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c protocols/viewporter-protocol.c -o $@

$(BUILD_DIR)/fractional-scale-v1-protocol.o: protocols/fractional-scale-v1-protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c protocols/fractional-scale-v1-protocol.c -o $@

$(BUILD_DIR)/tearing-control-v1-protocol.o: protocols/tearing-control-v1-protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c protocols/tearing-control-v1-protocol.c -o $@

DEPS = $(LIB_OBJS:.o=.d) $(BUILD_DIR)/main.d
-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d $(BUILD_DIR)/*.gch $(LIB_TARGET) $(TARGET) $(SHADERS)

.PHONY: all lib shaders pch clean
