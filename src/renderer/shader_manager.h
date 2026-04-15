#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include <string>
#include <vector>

// Include GLEW properly to avoid conflicts with system OpenGL headers
#define GL_GLEW_PROTOTYPES 1
#include <GL/glew.h>

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    // Load and compile a shader from file
    GLuint load_shader(GLenum type, const std::string& filename);

    // Load and compile a shader from source string
    GLuint compile_shader(GLenum type, const std::string& source, const std::string& name);

    // Link shaders into a program
    GLuint link_program(const std::vector<GLuint>& shaders, const std::string& program_name);

    // Load a compute shader from file
    GLuint load_compute_program(const std::string& filename);

    // Load vertex and fragment shaders from files
    GLuint load_render_program(const std::string& vertex_file, const std::string& fragment_file);

    // Check for OpenGL errors
    static bool check_gl_error(const std::string& operation);

    // Get shader compilation/linking log
    static std::string get_shader_info_log(GLuint shader);
    static std::string get_program_info_log(GLuint program);

private:
    std::vector<GLuint> created_shaders;
    std::vector<GLuint> created_programs;

    // Read file contents
    std::string read_file(const std::string& filename);
};

#endif // SHADER_MANAGER_H