#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<uint32_t> Shader::load_spirv_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    size_t fileSize = (size_t)file.tellg();
    if (fileSize == 0 || fileSize % 4 != 0) {
        return {};
    }

    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return buffer;
}

std::vector<uint32_t> Shader::compile_glsl_to_spirv(const std::string& filepath, const std::string& stage) {
    std::string spv_path = filepath + ".spv";

    // If source file doesn't exist, try loading precompiled .spv directly
    if (!fs::exists(filepath)) {
        if (fs::exists(spv_path)) {
            return load_spirv_file(spv_path);
        }
        std::cerr << "[Shader] Error: Source shader file not found: " << filepath << std::endl;
        return {};
    }

    // Check if recompilation is necessary
    bool needs_compile = true;
    if (fs::exists(spv_path)) {
        auto src_time = fs::last_write_time(filepath);
        auto spv_time = fs::last_write_time(spv_path);
        if (spv_time >= src_time) {
            needs_compile = false;
        }
    }

    if (needs_compile) {
        // Compile using glslc (or fallback to glslangValidator)
        std::string cmd = "glslc -fshader-stage=" + stage + " \"" + filepath + "\" -o \"" + spv_path + "\" 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[256];
            std::string output;
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            int ret = pclose(pipe);
            if (ret != 0) {
                std::cerr << "[Shader] glslc compilation failed for " << filepath << ":\n" << output << std::endl;
                // Try fallback to precompiled .spv if available
                if (fs::exists(spv_path)) {
                    return load_spirv_file(spv_path);
                }
                return {};
            }
        }
    }

    return load_spirv_file(spv_path);
}

VkShaderModule Shader::create_shader_module(VkDevice device, const std::vector<uint32_t>& code) {
    if (code.empty() || device == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "[Shader] Failed to create VkShaderModule." << std::endl;
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

Shader::Shader(VkDevice device, const std::string& vert_path, const std::string& frag_path) {
    load(device, vert_path, frag_path);
}

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : device_(other.device_),
      vert_module_(other.vert_module_),
      frag_module_(other.frag_module_),
      vert_path_(std::move(other.vert_path_)),
      frag_path_(std::move(other.frag_path_)) {
    other.device_ = VK_NULL_HANDLE;
    other.vert_module_ = VK_NULL_HANDLE;
    other.frag_module_ = VK_NULL_HANDLE;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        device_ = other.device_;
        vert_module_ = other.vert_module_;
        frag_module_ = other.frag_module_;
        vert_path_ = std::move(other.vert_path_);
        frag_path_ = std::move(other.frag_path_);

        other.device_ = VK_NULL_HANDLE;
        other.vert_module_ = VK_NULL_HANDLE;
        other.frag_module_ = VK_NULL_HANDLE;
    }
    return *this;
}

bool Shader::load(VkDevice device, const std::string& vert_path, const std::string& frag_path) {
    destroy();
    device_ = device;
    vert_path_ = vert_path;
    frag_path_ = frag_path;

    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    auto vert_code = compile_glsl_to_spirv(vert_path_, "vertex");
    auto frag_code = compile_glsl_to_spirv(frag_path_, "fragment");

    if (vert_code.empty() || frag_code.empty()) {
        std::cerr << "[Shader] Failed to compile or load SPIR-V for: "
                  << vert_path_ << " / " << frag_path_ << std::endl;
        return false;
    }

    vert_module_ = create_shader_module(device_, vert_code);
    frag_module_ = create_shader_module(device_, frag_code);

    if (vert_module_ == VK_NULL_HANDLE || frag_module_ == VK_NULL_HANDLE) {
        destroy();
        return false;
    }

    std::cout << "[Shader] Successfully loaded Vulkan SPIR-V modules: "
              << vert_path_ << " and " << frag_path_ << std::endl;
    return true;
}

bool Shader::reload() {
    if (device_ == VK_NULL_HANDLE || vert_path_.empty() || frag_path_.empty()) {
        return false;
    }

    std::cout << "[Shader] Recompiling " << vert_path_ << " & " << frag_path_ << "..." << std::endl;

    // Force recompilation
    std::string vert_spv = vert_path_ + ".spv";
    std::string frag_spv = frag_path_ + ".spv";
    fs::remove(vert_spv);
    fs::remove(frag_spv);

    auto vert_code = compile_glsl_to_spirv(vert_path_, "vertex");
    auto frag_code = compile_glsl_to_spirv(frag_path_, "fragment");

    if (vert_code.empty() || frag_code.empty()) {
        std::cerr << "[Shader] Hot-reload failed: Compilation error. Keeping active pipeline modules." << std::endl;
        return false;
    }

    VkShaderModule new_vert = create_shader_module(device_, vert_code);
    VkShaderModule new_frag = create_shader_module(device_, frag_code);

    if (new_vert == VK_NULL_HANDLE || new_frag == VK_NULL_HANDLE) {
        if (new_vert != VK_NULL_HANDLE) vkDestroyShaderModule(device_, new_vert, nullptr);
        if (new_frag != VK_NULL_HANDLE) vkDestroyShaderModule(device_, new_frag, nullptr);
        return false;
    }

    // Destroy old modules and swap
    if (vert_module_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert_module_, nullptr);
    if (frag_module_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, frag_module_, nullptr);

    vert_module_ = new_vert;
    frag_module_ = new_frag;

    std::cout << "[Shader] Hot-reload SUCCESSFUL!" << std::endl;
    return true;
}

void Shader::destroy() {
    if (device_ != VK_NULL_HANDLE) {
        if (vert_module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vert_module_, nullptr);
            vert_module_ = VK_NULL_HANDLE;
        }
        if (frag_module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, frag_module_, nullptr);
            frag_module_ = VK_NULL_HANDLE;
        }
    }
}
