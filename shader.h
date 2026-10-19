#ifndef SHADER_H
#define SHADER_H

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <string>

// -----------------------------------------------------------------------------
// Standalone Functions
// -----------------------------------------------------------------------------

// Reads and compiles a shader file from disk, reporting line-accurate error messages
GLuint compile_shader_file(GLenum type, const std::string& filepath);

// Compiles a shader from an in-memory string
GLuint compile_shader_source(GLenum type, const std::string& source, const std::string& debug_name = "in-memory");

// Links vertex and fragment shader files into a program
GLuint create_program_from_files(const std::string& vert_path, const std::string& frag_path);

// -----------------------------------------------------------------------------
// RAII Shader Class with Hot-Reloading Support
// -----------------------------------------------------------------------------
class Shader {
public:
    Shader() = default;
    Shader(const std::string& vert_path, const std::string& frag_path);
    ~Shader();

    // Movable, non-copyable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Load from disk files
    bool load_from_files(const std::string& vert_path, const std::string& frag_path);

    // Hot-reload: re-reads files from disk and recompiles.
    // If compilation succeeds, swaps to the new shader seamlessly.
    // If compilation fails, keeps the previous working shader active and logs the exact error.
    bool reload();

    // Bind shader for rendering (glUseProgram)
    void use() const;

    // Destroy the program
    void destroy();

    [[nodiscard]] bool is_valid() const noexcept { return program_id != 0; }
    [[nodiscard]] GLuint id() const noexcept { return program_id; }
    [[nodiscard]] const std::string& get_vertex_path() const noexcept { return vert_filepath; }
    [[nodiscard]] const std::string& get_fragment_path() const noexcept { return frag_filepath; }

    // Uniform helpers
    void set_int(const std::string& name, int value) const;
    void set_float(const std::string& name, float value) const;
    void set_vec2(const std::string& name, float x, float y) const;
    void set_vec3(const std::string& name, float x, float y, float z) const;
    void set_vec4(const std::string& name, float x, float y, float z, float w) const;
    void set_mat4(const std::string& name, const float* mat4_ptr) const;

private:
    GLuint      program_id = 0;
    std::string vert_filepath;
    std::string frag_filepath;
};

#endif // SHADER_H
