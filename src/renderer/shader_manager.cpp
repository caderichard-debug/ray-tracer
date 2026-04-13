#include "shader_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>

ShaderManager::ShaderManager() : base_path("src/shaders/") {}

ShaderManager::~ShaderManager() {
    // Clean up any remaining resources
}

std::string ShaderManager::read_shader_file(const std::string& filename) {
    std::string filepath = base_path + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return "";
    }

    std::stringstream stream;
    stream << file.rdbuf();
    file.close();

    return stream.str();
}

bool ShaderManager::check_compile_errors(GLuint shader, const std::string& type) {
    GLint success;
    GLchar info_log[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(shader, 1024, nullptr, info_log);
        std::cerr << "Shader compilation error (" << type << "):\n" << info_log << std::endl;
        return false;
    }

    return true;
}

bool ShaderManager::check_link_errors(GLuint program) {
    GLint success;
    GLchar info_log[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(program, 1024, nullptr, info_log);
        std::cerr << "Program linking error:\n" << info_log << std::endl;
        return false;
    }

    return true;
}

GLuint ShaderManager::load_compute_shader(const std::string& filename) {
    std::string source = read_shader_file(filename);
    if (source.empty()) {
        return 0;
    }

    const char* src_cstr = source.c_str();
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src_cstr, nullptr);
    glCompileShader(shader);

    if (!check_compile_errors(shader, "compute")) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint ShaderManager::load_vertex_shader(const std::string& filename) {
    std::string source = read_shader_file(filename);
    if (source.empty()) {
        return 0;
    }

    const char* src_cstr = source.c_str();
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader, 1, &src_cstr, nullptr);
    glCompileShader(shader);

    if (!check_compile_errors(shader, "vertex")) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint ShaderManager::load_fragment_shader(const std::string& filename) {
    std::string source = read_shader_file(filename);
    if (source.empty()) {
        return 0;
    }

    const char* src_cstr = source.c_str();
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader, 1, &src_cstr, nullptr);
    glCompileShader(shader);

    if (!check_compile_errors(shader, "fragment")) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint ShaderManager::create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    if (!check_link_errors(program)) {
        glDeleteProgram(program);
        return 0;
    }

    // Shaders can be deleted after linking
    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);

    return program;
}

GLuint ShaderManager::create_compute_program(GLuint compute_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, compute_shader);
    glLinkProgram(program);

    if (!check_link_errors(program)) {
        glDeleteProgram(program);
        return 0;
    }

    glDetachShader(program, compute_shader);

    return program;
}

void ShaderManager::delete_shader(GLuint shader) {
    glDeleteShader(shader);
}

void ShaderManager::delete_program(GLuint program) {
    glDeleteProgram(program);
}
