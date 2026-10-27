#pragma once

// Vulkan & Wayland
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

// Linux System APIs
#include <linux/input-event-codes.h>
#include <linux/joystick.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

// Standard C++ Library
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <map>
#include <array>
#include <optional>
#include <filesystem>
#include <algorithm>
#include <utility>
#include <iomanip>
