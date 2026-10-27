#ifndef SHADER_HPP
#define SHADER_HPP

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

// =============================================================================
// Vulkan Shader Module & SPIR-V Pipeline Manager
// =============================================================================
//
// Compiles standard GLSL into SPIR-V using glslc/glslangValidator, creates
// VkShaderModules, and supports runtime hot-reloading.
// =============================================================================
class Shader {
public:
    Shader() = default;
    Shader(VkDevice device, const std::string& vert_path, const std::string& frag_path);
    ~Shader();

    // Movable, non-copyable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool load(VkDevice device, const std::string& vert_path, const std::string& frag_path);
    bool reload();
    void destroy();

    [[nodiscard]] bool is_valid() const noexcept {
        return vert_module_ != VK_NULL_HANDLE && frag_module_ != VK_NULL_HANDLE;
    }

    [[nodiscard]] VkShaderModule vert_module() const noexcept { return vert_module_; }
    [[nodiscard]] VkShaderModule frag_module() const noexcept { return frag_module_; }
    [[nodiscard]] const std::string& get_vertex_path() const noexcept { return vert_path_; }
    [[nodiscard]] const std::string& get_fragment_path() const noexcept { return frag_path_; }

    static std::vector<uint32_t> load_spirv_file(const std::string& filepath);
    static std::vector<uint32_t> compile_glsl_to_spirv(const std::string& filepath, const std::string& stage);
    static VkShaderModule create_shader_module(VkDevice device, const std::vector<uint32_t>& code);

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkShaderModule vert_module_{VK_NULL_HANDLE};
    VkShaderModule frag_module_{VK_NULL_HANDLE};
    std::string vert_path_;
    std::string frag_path_;
};

#endif // SHADER_HPP
