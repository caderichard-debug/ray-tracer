#ifndef GPU_RENDERER_H
#define GPU_RENDERER_H

#include <memory>
#include <string>
#include <vector>
#include "scene/scene.h"
#include "camera/camera.h"
#include "renderer/performance.h"

class ShaderManager;

class GPURenderer {
public:
    GPURenderer();
    ~GPURenderer();

    // Initialize OpenGL and GPU resources
    bool initialize(int width, int height);

    // Clean up GPU resources
    void cleanup();

    // Set the scene to render
    void set_scene(const std::shared_ptr<Scene>& scene);

    // Render the scene from camera viewpoint
    void render(const Camera& camera, std::vector<std::vector<Color>>& framebuffer);

    // Check if GPU renderer is available
    bool is_available() const { return initialized; }

    // Get performance statistics
    const PerformanceTracker& get_performance() const { return performance; }

    // Resize render target
    void resize(int width, int height);

private:
    // OpenGL initialization
    bool init_opengl();
    bool init_shaders();
    bool init_buffers();

    // Scene upload to GPU
    void upload_scene();
    void upload_camera(const Camera& camera);

    // Rendering
    void dispatch_compute();
    void read_framebuffer(std::vector<std::vector<Color>>& framebuffer);

    // Shader programs
    GLuint compute_program;
    GLuint vertex_program;
    GLuint fragment_program;

    // GPU buffers (SSBOs)
    GLuint sphere_buffer;
    GLuint triangle_buffer;
    GLuint material_buffer;
    GLuint light_buffer;
    GLuint camera_buffer;
    GLuint output_texture;
    GLuint vao;  // For screen quad

    // Scene data
    std::shared_ptr<Scene> scene;

    // Dimensions
    int width, height;

    // State
    bool initialized;
    bool scene_uploaded;

    // Shader manager
    std::unique_ptr<ShaderManager> shader_manager;

    // Performance tracking
    PerformanceTracker performance;

    // Timer queries for GPU timing
    GLuint timer_queries[2];
    bool timer_active;
};

#endif // GPU_RENDERER_H