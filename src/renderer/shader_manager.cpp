#include "shader_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cerrno>

ShaderManager::ShaderManager() {
}

ShaderManager::~ShaderManager() {
    // Clean up shaders
    for (GLuint shader : created_shaders) {
        glDeleteShader(shader);
    }
    for (GLuint program : created_programs) {
        glDeleteProgram(program);
    }
}

std::string ShaderManager::read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ShaderManager::get_shader_info_log(GLuint shader) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length == 0) return "";

    std::vector<char> log(length);
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return std::string(log.begin(), log.end());
}

std::string ShaderManager::get_program_info_log(GLuint program) {
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    if (length == 0) return "";

    std::vector<char> log(length);
    glGetProgramInfoLog(program, length, nullptr, log.data());
    return std::string(log.begin(), log.end());
}

bool ShaderManager::check_gl_error(const std::string& operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error during " << operation << ": " << error << std::endl;
        return true;
    }
    return false;
}

GLuint ShaderManager::compile_shader(GLenum type, const std::string& source, const std::string& name) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        std::cerr << "Failed to create shader: " << name << std::endl;
        return 0;
    }

    const char* source_cstr = source.c_str();
    glShaderSource(shader, 1, &source_cstr, nullptr);
    glCompileShader(shader);

    GLint compile_status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    if (compile_status == GL_FALSE) {
        std::cerr << "Failed to compile shader: " << name << std::endl;
        std::string log = get_shader_info_log(shader);
        if (!log.empty()) {
            std::cerr << "Compilation log:\n" << log << std::endl;
        }
        glDeleteShader(shader);
        return 0;
    }

    created_shaders.push_back(shader);
    std::cout << "✓ Compiled shader: " << name << std::endl;
    return shader;
}

GLuint ShaderManager::load_shader(GLenum type, const std::string& filename) {
    std::string source = read_file(filename);
    if (source.empty()) {
        return 0;
    }

    std::string shader_type = (type == GL_VERTEX_SHADER) ? "vertex" :
                             (type == GL_FRAGMENT_SHADER) ? "fragment" :
                             (type == GL_COMPUTE_SHADER) ? "compute" : "unknown";

    return compile_shader(type, source, filename + " (" + shader_type + ")");
}

GLuint ShaderManager::link_program(const std::vector<GLuint>& shaders, const std::string& program_name) {
    GLuint program = glCreateProgram();
    if (program == 0) {
        std::cerr << "Failed to create program: " << program_name << std::endl;
        return 0;
    }

    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }

    glLinkProgram(program);

    GLint link_status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);

    if (link_status == GL_FALSE) {
        std::cerr << "Failed to link program: " << program_name << std::endl;
        std::string log = get_program_info_log(program);
        if (!log.empty()) {
            std::cerr << "Link log:\n" << log << std::endl;
        }
        glDeleteProgram(program);
        return 0;
    }

    created_programs.push_back(program);
    std::cout << "✓ Linked program: " << program_name << std::endl;
    return program;
}

GLuint ShaderManager::load_compute_program(const std::string& filename) {
    GLuint shader = load_shader(GL_COMPUTE_SHADER, filename);
    if (shader == 0) {
        return 0;
    }

    std::vector<GLuint> shaders = {shader};
    return link_program(shaders, filename);
}

GLuint ShaderManager::load_render_program(const std::string& vertex_file, const std::string& fragment_file) {
    GLuint vertex_shader = load_shader(GL_VERTEX_SHADER, vertex_file);
    if (vertex_shader == 0) {
        return 0;
    }

    GLuint fragment_shader = load_shader(GL_FRAGMENT_SHADER, fragment_file);
    if (fragment_shader == 0) {
        return 0;
    }

    std::vector<GLuint> shaders = {vertex_shader, fragment_shader};
    std::string program_name = vertex_file + " + " + fragment_file;
    return link_program(shaders, program_name);
}