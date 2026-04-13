#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include <string>
#include <GL/glew.h>

// Shader Manager handles loading, compiling, and linking OpenGL shaders
class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    // Load and compile a compute shader from file
    GLuint load_compute_shader(const std::string& filename);

    // Load and compile a vertex shader from file
    GLuint load_vertex_shader(const std::string& filename);

    // Load and compile a fragment shader from file
    GLuint load_fragment_shader(const std::string& filename);

    // Create a program from vertex and fragment shaders
    GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

    // Create a compute program
    GLuint create_compute_program(GLuint compute_shader);

    // Read shader source from file
    std::string read_shader_file(const std::string& filename);

    // Check for shader compilation errors
    bool check_compile_errors(GLuint shader, const std::string& type);

    // Check for program linking errors
    bool check_link_errors(GLuint program);

    // Delete a shader
    void delete_shader(GLuint shader);

    // Delete a program
    void delete_program(GLuint program);

private:
    std::string base_path;
};

#endif // SHADER_MANAGER_H
