#include "shader.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

GLuint compile_shader_source(GLenum type, const std::string& source, const std::string& debug_name) {
    GLuint shader = glCreateShader(type);
    const char* src_ptr = source.c_str();
    glShaderSource(shader, 1, &src_ptr, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> info_log(log_length > 0 ? log_length : 1024);
        glGetShaderInfoLog(shader, (GLsizei)info_log.size(), nullptr, info_log.data());

        const char* type_str = (type == GL_VERTEX_SHADER) ? "Vertex" : (type == GL_FRAGMENT_SHADER) ? "Fragment" : "Shader";
        std::cerr << "\n[GLSL Compilation Error] (" << type_str << " in " << debug_name << "):\n"
                  << info_log.data() << std::endl;

        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint compile_shader_file(GLenum type, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Shader Error] Failed to open shader file: " << filepath << std::endl;
        return 0;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    return compile_shader_source(type, source, filepath);
}

GLuint create_program_from_files(const std::string& vert_path, const std::string& frag_path) {
    GLuint vert = compile_shader_file(GL_VERTEX_SHADER, vert_path);
    if (!vert) return 0;

    GLuint frag = compile_shader_file(GL_FRAGMENT_SHADER, frag_path);
    if (!frag) {
        glDeleteShader(vert);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> info_log(log_length > 0 ? log_length : 1024);
        glGetProgramInfoLog(program, (GLsizei)info_log.size(), nullptr, info_log.data());
        std::cerr << "\n[GLSL Program Link Error] (" << vert_path << " + " << frag_path << "):\n"
                  << info_log.data() << std::endl;
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}

// -----------------------------------------------------------------------------
// Shader Class Implementation
// -----------------------------------------------------------------------------
Shader::Shader(const std::string& vert_path, const std::string& frag_path) {
    load_from_files(vert_path, frag_path);
}

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : program_id(other.program_id),
      vert_filepath(std::move(other.vert_filepath)),
      frag_filepath(std::move(other.frag_filepath)) {
    other.program_id = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        program_id = other.program_id;
        vert_filepath = std::move(other.vert_filepath);
        frag_filepath = std::move(other.frag_filepath);
        other.program_id = 0;
    }
    return *this;
}

bool Shader::load_from_files(const std::string& vert_path, const std::string& frag_path) {
    GLuint new_prog = create_program_from_files(vert_path, frag_path);
    if (!new_prog) {
        return false;
    }

    destroy();
    program_id = new_prog;
    vert_filepath = vert_path;
    frag_filepath = frag_path;
    std::cout << "[Shader] Successfully loaded: " << vert_path << " and " << frag_path << std::endl;
    return true;
}

bool Shader::reload() {
    if (vert_filepath.empty() || frag_filepath.empty()) {
        std::cerr << "[Shader] Cannot reload: no filepaths set." << std::endl;
        return false;
    }

    std::cout << "[Shader] Hot-reloading " << vert_filepath << " & " << frag_filepath << "..." << std::endl;
    GLuint new_prog = create_program_from_files(vert_filepath, frag_filepath);
    if (!new_prog) {
        std::cerr << "[Shader] Hot-reload failed. Preserving previous working shader program." << std::endl;
        return false;
    }

    // Hot-swap
    destroy();
    program_id = new_prog;
    std::cout << "[Shader] Hot-reload SUCCESSFUL!" << std::endl;
    return true;
}

void Shader::use() const {
    if (program_id != 0) {
        glUseProgram(program_id);
    }
}

void Shader::destroy() {
    if (program_id != 0) {
        glDeleteProgram(program_id);
        program_id = 0;
    }
}

void Shader::set_int(const std::string& name, int value) const {
    GLint loc = glGetUniformLocation(program_id, name.c_str());
    if (loc >= 0) glUniform1i(loc, value);
}

void Shader::set_float(const std::string& name, float value) const {
    GLint loc = glGetUniformLocation(program_id, name.c_str());
    if (loc >= 0) glUniform1f(loc, value);
}

void Shader::set_vec2(const std::string& name, float x, float y) const {
    GLint loc = glGetUniformLocation(program_id, name.c_str());
    if (loc >= 0) glUniform2f(loc, x, y);
}

void Shader::set_vec3(const std::string& name, float x, float y, float z) const {
    GLint loc = glGetUniformLocation(program_id, name.c_str());
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

void Shader::set_vec4(const std::string& name, float x, float y, float z, float w) const {
    GLint loc = glGetUniformLocation(program_id, name.c_str());
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

void Shader::set_mat4(const std::string& name, const float* mat4_ptr) const {
    GLint loc = glGetUniformLocation(program_id, name.c_str());
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, mat4_ptr);
}
