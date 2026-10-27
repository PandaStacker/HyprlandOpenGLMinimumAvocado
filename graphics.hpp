#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>
#include <memory>
#include <vector>
#include <cstdint>

class WaylandWindow;
class Shader;
class Client;

// =============================================================================
// Vulkan Graphics Subsystem for Wayland
// =============================================================================
//
// Strictly responsible for rendering and GPU resources.
// Implements a hardware-accelerated Vulkan pipeline directly atop
// Wayland (wl_surface -> VK_KHR_wayland_surface -> Swapchain).
// =============================================================================
class Graphics {
public:
    explicit Graphics(WaylandWindow& window);
    ~Graphics();

    // Non-copyable, movable
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;
    Graphics(Graphics&&) noexcept;
    Graphics& operator=(Graphics&&) noexcept;

    // Hot-reload shaders
    void reload_shaders();

    // Resize viewport & swapchain
    void resize(int width, int height);

    // Render pass (Reads Client as a pure observer)
    void render(const Client& client);

    [[nodiscard]] bool is_ready() const noexcept { return ready_; }

private:
    void init_vulkan();
    void create_instance();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_swapchain();
    void create_image_views();
    void create_render_pass();
    void create_pipeline();
    void create_framebuffers();
    void create_command_pool();
    void create_vertex_buffer();
    void create_command_buffers();
    void create_sync_objects();

    void cleanup_swapchain();
    void recreate_swapchain();

    uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    WaylandWindow* window_{nullptr};
    bool ready_{false};

    VkInstance instance_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue graphics_queue_{VK_NULL_HANDLE};
    VkQueue present_queue_{VK_NULL_HANDLE};
    uint32_t graphics_family_{0};
    uint32_t present_family_{0};

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    VkFormat swapchain_image_format_{VK_FORMAT_B8G8R8A8_SRGB};
    VkExtent2D swapchain_extent_{};

    VkRenderPass render_pass_{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};

    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool command_pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_;

    VkBuffer vertex_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory vertex_buffer_memory_{VK_NULL_HANDLE};

    VkSemaphore image_available_semaphore_{VK_NULL_HANDLE};
    VkSemaphore render_finished_semaphore_{VK_NULL_HANDLE};
    VkFence in_flight_fence_{VK_NULL_HANDLE};

    std::unique_ptr<Shader> shader_;
};

#endif // GRAPHICS_HPP
