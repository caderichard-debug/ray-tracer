#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

// Include GLEW before SDL to avoid header conflicts
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// Window dimensions
const int WIDTH = 800;
const int HEIGHT = 450;

// Fragment shader for ray tracing (works with OpenGL 2.0+)
const char* fragment_shader_source = R"(
#version 120

// Uniforms
uniform vec2 resolution;
uniform float time;

// Simple ray-sphere intersection
bool hit_sphere(vec3 origin, vec3 direction, vec3 center, float radius, inout float t) {
    vec3 oc = origin - center;
    float a = dot(direction, direction);
    float b = dot(oc, direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - a * c;

    if (discriminant < 0.0) {
        return false;
    }

    float sqrt_d = sqrt(discriminant);
    float root = (-b - sqrt_d) / a;

    if (root < 0.001 || root > t) {
        root = (-b + sqrt_d) / a;
        if (root < 0.001 || root > t) {
            return false;
        }
    }

    t = root;
    return true;
}

// Simple scene rendering
vec3 ray_color(vec3 origin, vec3 direction) {
    // Background gradient
    vec3 unit_dir = normalize(direction);
    float t = 0.5 * (unit_dir.y + 1.0);
    vec3 background = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);

    // Check intersection with sphere
    vec3 sphere_center = vec3(0.0, 0.0, -1.0);
    float sphere_radius = 0.5;

    float t_max = 1000.0;
    if (hit_sphere(origin, direction, sphere_center, sphere_radius, t_max)) {
        // Calculate normal at hit point
        vec3 hit_point = origin + t_max * direction;
        vec3 normal = normalize(hit_point - sphere_center);

        // Simple lighting
        vec3 light_dir = normalize(vec3(1.0, 1.0, -1.0));
        float diff = max(dot(normal, light_dir), 0.0);

        // Sphere color with lighting
        vec3 color = vec3(0.8, 0.3, 0.3) * (diff + 0.1); // Red sphere
        return color;
    }

    return background;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y; // Flip Y

    // Simple camera
    vec3 origin = vec3(0.0, 0.0, 2.0);
    vec3 lookat = vec3(0.0, 0.0, -1.0);
    vec3 vup = vec3(0.0, 1.0, 0.0);

    vec3 w = normalize(origin - lookat);
    vec3 u = normalize(cross(vup, w));
    vec3 v = cross(w, u);

    float vfov = 90.0;
    float aspect_ratio = resolution.x / resolution.y;
    float theta = radians(vfov); // Note: radians() is GLSL 3.0+, need workaround for GLSL 1.2
    float h = tan(theta * 0.0174533); // Approximate radians
    float viewport_height = 2.0 * h;
    float viewport_width = aspect_ratio * viewport_height;

    vec3 horizontal = viewport_width * u;
    vec3 vertical = viewport_height * v;
    vec3 lower_left_corner = origin - horizontal * 0.5 - vertical * 0.5 - w;

    vec3 direction = lower_left_corner + uv.x * horizontal + uv.y * vertical - origin;

    // Trace ray and get color
    vec3 color = ray_color(origin, direction);

    // Output color
    gl_FragColor = vec4(color, 1.0);
}
)";

// Vertex shader for fullscreen quad
const char* vertex_shader_source = R"(
#version 120

attribute vec2 position;
attribute vec2 texCoord;

varying vec2 uv;

void main() {
    uv = texCoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

// Shader compilation helper
GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());

        std::cerr << "Failed to compile shader:" << std::endl;
        std::cerr << log.data() << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// Program linking helper
GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(log_length);
        glGetProgramInfoLog(program, log_length, nullptr, log.data());

        std::cerr << "Failed to link program:" << std::endl;
        std::cerr << log.data() << std::endl;

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=== Legacy GPU Ray Tracer (Fragment Shaders) ===" << std::endl;
    std::cout << "Compatible with OpenGL 2.0+" << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create OpenGL window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Legacy GPU Ray Tracer - Fragment Shaders",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Failed to create OpenGL window" << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create OpenGL context
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "✓ OpenGL window created" << std::endl;

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(err) << std::endl;
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Check OpenGL version
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL Version: " << version << std::endl;

    const GLubyte* shader_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    std::cout << "GLSL Version: " << shader_version << std::endl;

    // Compile shaders
    std::cout << "Compiling shaders..." << std::endl;
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    if (vertex_shader == 0) {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GLuint program = link_program(vertex_shader, fragment_shader);
    if (program == 0) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "✓ Shaders compiled and linked" << std::endl;

    // Create fullscreen quad
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    float quad_vertices[] = {
        // Position  // TexCoord
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Position attribute
    GLint pos_loc = glGetAttribLocation(program, "position");
    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(pos_loc);

    // TexCoord attribute
    GLint tex_loc = glGetAttribLocation(program, "texCoord");
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(tex_loc);

    // Get uniform locations
    GLint resolution_loc = glGetUniformLocation(program, "resolution");
    GLint time_loc = glGetUniformLocation(program, "time");

    std::cout << "\n=== Starting Legacy GPU Ray Tracer ===" << std::endl;
    std::cout << "You should see a red sphere on a gradient background" << std::endl;
    std::cout << "Controls: ESC to quit" << std::endl;
    std::cout << "==============================\n" << std::endl;

    // Main loop
    bool running = true;
    SDL_Event event;

    auto start_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        // Calculate time
        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(current_time - start_time).count();

        // Render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);

        // Set uniforms
        glUniform2f(resolution_loc, (float)WIDTH, (float)HEIGHT);
        glUniform1f(time_loc, time);

        // Render fullscreen quad
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        SDL_GL_SwapWindow(window);

        // Calculate FPS
        frame_count++;
        if (frame_count % 30 == 0) {
            auto end_time = std::chrono::high_resolution_clock::now();
            float elapsed = std::chrono::duration<float>(end_time - start_time).count();
            float fps = frame_count / elapsed;
            std::cout << "FPS: " << fps << "\r" << std::flush;
        }
    }

    std::cout << "\n=== Exiting ===" << std::endl;

    // Cleanup
    glDeleteProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Legacy GPU Ray Tracer shut down successfully" << std::endl;
    return 0;
}
