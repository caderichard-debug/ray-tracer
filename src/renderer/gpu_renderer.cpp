#include "gpu_renderer.h"
#include "shader_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

// Structure definitions for GPU buffers (must match shader layouts)
struct GPUCamera {
    float position[4];
    float lookat[4];
    float vup[4];
    float vfov;
    float aspect_ratio;
    float padding[2];
};

struct GPUSphere {
    float center[4];
    float radius;
    int material_id;
    float padding[3];
};

struct GPUTriangle {
    float v0[4];
    float v1[4];
    float v2[4];
    float normal[4];
    int material_id;
    float padding[3];
};

struct GPUMaterial {
    float albedo[4];
    float fuzz;
    int type;
    float padding[2];
};

struct GPULight {
    float position[4];
    float intensity[4];
};

GPURenderer::GPURenderer()
    : compute_program(0), vertex_program(0), fragment_program(0),
      sphere_buffer(0), triangle_buffer(0), material_buffer(0),
      light_buffer(0), camera_buffer(0), output_texture(0), vao(0),
      width(0), height(0), initialized(false), scene_uploaded(false),
      timer_active(false) {
    shader_manager = std::make_unique<ShaderManager>();
}

GPURenderer::~GPURenderer() {
    cleanup();
}

bool GPURenderer::initialize(int w, int h) {
    if (initialized) {
        std::cerr << "GPU renderer already initialized" << std::endl;
        return true;
    }

    width = w;
    height = h;

    std::cout << "Initializing GPU renderer..." << std::endl;

    if (!init_opengl()) {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        return false;
    }

    if (!init_shaders()) {
        std::cerr << "Failed to initialize shaders" << std::endl;
        return false;
    }

    if (!init_buffers()) {
        std::cerr << "Failed to initialize buffers" << std::endl;
        return false;
    }

    // Create timer queries for performance measurement
    glGenQueries(2, timer_queries);

    initialized = true;
    std::cout << "✓ GPU renderer initialized" << std::endl;
    return true;
}

bool GPURenderer::init_opengl() {
    // Check OpenGL version
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL Version: " << version << std::endl;

    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    if (major < 4 || (major == 4 && minor < 3)) {
        std::cerr << "OpenGL 4.3+ required, got " << major << "." << minor << std::endl;
        return false;
    }

    // Check compute shader support
    GLint max_compute_work_group_count[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_compute_work_group_count[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &max_compute_work_group_count[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &max_compute_work_group_count[2]);

    std::cout << "Max compute work group count: ["
              << max_compute_work_group_count[0] << ", "
              << max_compute_work_group_count[1] << ", "
              << max_compute_work_group_count[2] << "]" << std::endl;

    return true;
}

bool GPURenderer::init_shaders() {
    // TODO: Load actual shader files once created
    // For now, create a placeholder
    std::cout << "Shader loading not yet implemented" << std::endl;
    return true;
}

bool GPURenderer::init_buffers() {
    // Create output texture
    glGenTextures(1, &output_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, output_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create SSBOs
    glGenBuffers(1, &sphere_buffer);
    glGenBuffers(1, &triangle_buffer);
    glGenBuffers(1, &material_buffer);
    glGenBuffers(1, &light_buffer);
    glGenBuffers(1, &camera_buffer);

    // Create VAO for screen quad
    glGenVertexArrays(1, &vao);

    return true;
}

void GPURenderer::set_scene(const std::shared_ptr<Scene>& scene_ptr) {
    scene = scene_ptr;
    scene_uploaded = false;
}

void GPURenderer::upload_scene() {
    if (!scene || scene_uploaded) {
        return;
    }

    std::cout << "Uploading scene to GPU..." << std::endl;

    // TODO: Upload scene data to GPU buffers
    // This will be implemented once we have the compute shader

    scene_uploaded = true;
    std::cout << "✓ Scene uploaded to GPU" << std::endl;
}

void GPURenderer::upload_camera(const Camera& camera) {
    GPUCamera gpu_camera;

    // Copy camera data
    Vec3 pos = camera.get_origin();
    Vec3 look = camera.get_target();
    Vec3 up = camera.get_up();
    float vfov = camera.get_vfov();
    float aspect = camera.get_aspect();

    gpu_camera.position[0] = pos.x();
    gpu_camera.position[1] = pos.y();
    gpu_camera.position[2] = pos.z();
    gpu_camera.position[3] = 1.0f;

    gpu_camera.lookat[0] = look.x();
    gpu_camera.lookat[1] = look.y();
    gpu_camera.lookat[2] = look.z();
    gpu_camera.lookat[3] = 1.0f;

    gpu_camera.vup[0] = up.x();
    gpu_camera.vup[1] = up.y();
    gpu_camera.vup[2] = up.z();
    gpu_camera.vup[3] = 1.0f;

    gpu_camera.vfov = vfov;
    gpu_camera.aspect_ratio = aspect;
    gpu_camera.padding[0] = 0.0f;
    gpu_camera.padding[1] = 0.0f;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, camera_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GPUCamera), &gpu_camera, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, camera_buffer);
}

void GPURenderer::render(const Camera& camera, std::vector<std::vector<Color>>& framebuffer) {
    if (!initialized) {
        std::cerr << "GPU renderer not initialized" << std::endl;
        return;
    }

    if (!scene) {
        std::cerr << "No scene set" << std::endl;
        return;
    }

    // Start performance tracking
    performance.start_render();

    // Upload scene if needed
    if (!scene_uploaded) {
        upload_scene();
    }

    // Upload camera
    upload_camera(camera);

    // Start GPU timer
    if (!timer_active) {
        glBeginQuery(GL_TIME_ELAPSED, timer_queries[0]);
        timer_active = true;
    }

    // Dispatch compute shader
    dispatch_compute();

    // Stop GPU timer
    glEndQuery(GL_TIME_ELAPSED);

    // Read back framebuffer
    read_framebuffer(framebuffer);

    // Get GPU timing
    GLuint64 gpu_time_ns = 0;
    glGetQueryObjectui64v(timer_queries[0], GL_QUERY_RESULT, &gpu_time_ns);
    double gpu_time_ms = gpu_time_ns / 1e6;

    // Stop performance tracking
    performance.stop_render(width * height, gpu_time_ms);

    std::cout << "GPU render time: " << gpu_time_ms << " ms" << std::endl;
}

void GPURenderer::dispatch_compute() {
    // TODO: Implement actual compute shader dispatch
    // This will be implemented once we have the compute shader
    std::cout << "Compute dispatch not yet implemented" << std::endl;
}

void GPURenderer::read_framebuffer(std::vector<std::vector<Color>>& framebuffer) {
    // Resize framebuffer if needed
    if (framebuffer.size() != static_cast<size_t>(height) ||
        framebuffer[0].size() != static_cast<size_t>(width)) {
        framebuffer.resize(height, std::vector<Color>(width));
    }

    // TODO: Read back from GPU texture
    // This will be implemented once we have the compute shader
    std::cout << "Framebuffer readback not yet implemented" << std::endl;
}

void GPURenderer::resize(int w, int h) {
    if (width == w && height == h) {
        return;
    }

    width = w;
    height = h;

    // Resize output texture
    glBindTexture(GL_TEXTURE_2D, output_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GPURenderer::cleanup() {
    if (!initialized) {
        return;
    }

    // Delete timer queries
    if (timer_active) {
        glDeleteQueries(2, timer_queries);
        timer_active = false;
    }

    // Delete buffers
    if (sphere_buffer) glDeleteBuffers(1, &sphere_buffer);
    if (triangle_buffer) glDeleteBuffers(1, &triangle_buffer);
    if (material_buffer) glDeleteBuffers(1, &material_buffer);
    if (light_buffer) glDeleteBuffers(1, &light_buffer);
    if (camera_buffer) glDeleteBuffers(1, &camera_buffer);

    // Delete texture
    if (output_texture) glDeleteTextures(1, &output_texture);

    // Delete VAO
    if (vao) glDeleteVertexArrays(1, &vao);

    // Delete programs (handled by shader manager)
    compute_program = 0;
    vertex_program = 0;
    fragment_program = 0;

    initialized = false;
    std::cout << "GPU renderer cleaned up" << std::endl;
}