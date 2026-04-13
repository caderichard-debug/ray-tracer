#ifndef GPU_RENDERER_H
#define GPU_RENDERER_H

#include "scene/scene.h"
#include "camera/camera.h"
#include "performance.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <string>

class GPURenderer {
public:
    GPURenderer();
    ~GPURenderer();

    // Initialize OpenGL context and resources
    bool initialize(SDL_Window* sdl_window, int width, int height);

    // Upload scene data to GPU
    void set_scene(const Scene& scene);

    // Render the scene with the given camera
    void render(const Camera& camera, const Scene& scene, int samples_per_pixel, int max_depth);

    // Get the rendered image data
    void get_framebuffer(unsigned char* data, int width, int height);

    // Get the texture ID for display
    GLuint get_texture() const { return output_texture; }

    // Render the scene (uses fragment shader approach)
    void render_fragment(const Camera& camera, int max_depth);

    // Display the rendered image to screen
    void display();

    // Clean up resources
    void cleanup();

    // Check if initialized
    bool is_initialized() const { return initialized; }

    // Get performance stats
    float get_last_render_time() const { return last_render_time; }
    uint64_t get_last_ray_count() const { return last_ray_count; }

private:
    // Initialize OpenGL context
    bool init_opengl();

    // Create shaders
    bool create_shaders();
    bool create_display_shaders(class ShaderManager& sm);

    // Create buffers and textures
    bool create_buffers(int width, int height);

    // Create quad for display
    bool create_display_quad();

    // Upload scene data to SSBOs
    void upload_scene_data(const Scene& scene);

    // Upload camera data to uniform buffer
    void upload_camera_data(const Camera& camera);

    // Dispatch compute shader
    void dispatch_compute(int samples_per_pixel, int max_depth);

    // OpenGL resources
    GLuint compute_program;
    GLuint display_program;
    GLuint output_texture;
    GLuint vao;
    GLuint vbo;

    // SSBOs
    GLuint sphere_ssbo;
    GLuint triangle_ssbo;
    GLuint material_ssbo;
    GLuint light_ssbo;
    GLuint camera_ubo;

    // Scene info
    int num_spheres;
    int num_triangles;
    int num_lights;

    // Dimensions
    int width;
    int height;

    // Performance tracking
    float last_render_time;
    uint64_t last_ray_count;
    PerformanceTracker perf_tracker;

    // State
    bool initialized;
    bool use_compute_shader; // Try compute shaders first, fallback to fragment

    // SDL window and OpenGL context (owned by main application)
    SDL_Window* sdl_window;
    SDL_GLContext gl_context;
    bool owns_context; // True if we created the context

    // Fragment shader program (fallback)
    GLuint fragment_program;
    GLuint vao_fallback;
    GLuint vbo_fallback;
};

#endif // GPU_RENDERER_H
