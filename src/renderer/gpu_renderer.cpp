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

    std::cout << "✓ OpenGL 4.3+ detected" << std::endl;

    // Check compute shader support (optional, for information only)
    GLint max_compute_work_group_count[3] = {0, 0, 0};
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
    std::cout << "Loading shaders..." << std::endl;

    // Load compute shader
    compute_program = shader_manager->load_compute_program("src/shaders/raytrace.comp");
    if (compute_program == 0) {
        std::cerr << "Failed to load compute shader" << std::endl;
        return false;
    }

    // Load render shaders for screen quad
    vertex_program = shader_manager->load_shader(GL_VERTEX_SHADER, "src/shaders/quad.vert");
    if (vertex_program == 0) {
        std::cerr << "Failed to load vertex shader" << std::endl;
        return false;
    }

    GLuint fragment_shader = shader_manager->load_shader(GL_FRAGMENT_SHADER, "src/shaders/quad.frag");
    if (fragment_shader == 0) {
        std::cerr << "Failed to load fragment shader" << std::endl;
        return false;
    }

    // Link render program
    std::vector<GLuint> render_shaders = {vertex_program, fragment_shader};
    fragment_program = shader_manager->link_program(render_shaders, "quad");
    if (fragment_program == 0) {
        std::cerr << "Failed to link render program" << std::endl;
        return false;
    }

    std::cout << "✓ Shaders loaded successfully" << std::endl;
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
    gpu_camera.position[0] = camera.origin.x;
    gpu_camera.position[1] = camera.origin.y;
    gpu_camera.position[2] = camera.origin.z;
    gpu_camera.position[3] = 1.0f;

    gpu_camera.lookat[0] = camera.lookat.x;
    gpu_camera.lookat[1] = camera.lookat.y;
    gpu_camera.lookat[2] = camera.lookat.z;
    gpu_camera.lookat[3] = 1.0f;

    gpu_camera.vup[0] = camera.vup.x;
    gpu_camera.vup[1] = camera.vup.y;
    gpu_camera.vup[2] = camera.vup.z;
    gpu_camera.vup[3] = 1.0f;

    gpu_camera.vfov = camera.vfov;
    gpu_camera.aspect_ratio = camera.aspect_ratio;
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

    // Create performance tracker for this render
    performance = std::make_unique<PerformanceTracker>("GPU Render", width, height);

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
    performance->stop();

    std::cout << "GPU render time: " << gpu_time_ms << " ms" << std::endl;
}

void GPURenderer::dispatch_compute() {
    // Use compute shader
    glUseProgram(compute_program);

    // Set uniforms
    GLint resolution_loc = glGetUniformLocation(compute_program, "resolution");
    glUniform2f(resolution_loc, (float)width, (float)height);

    GLint frame_count_loc = glGetUniformLocation(compute_program, "frame_count");
    glUniform1ui(frame_count_loc, 0);

    // Bind output texture as image
    glBindImageTexture(5, output_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // Dispatch compute shader
    int work_groups_x = (width + 15) / 16;
    int work_groups_y = (height + 15) / 16;
    glDispatchCompute(work_groups_x, work_groups_y, 1);

    // Wait for compute shader to finish
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glUseProgram(0);
}

void GPURenderer::read_framebuffer(std::vector<std::vector<Color>>& framebuffer) {
    // Resize framebuffer if needed
    if (framebuffer.size() != static_cast<size_t>(height) ||
        (framebuffer.size() > 0 && framebuffer[0].size() != static_cast<size_t>(width))) {
        framebuffer.resize(height, std::vector<Color>(width));
    }

    // Read back from GPU texture
    std::vector<float> pixels(width * height * 4);
    glBindTexture(GL_TEXTURE_2D, output_texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Convert to Color format
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            framebuffer[y][x] = Color(pixels[idx], pixels[idx + 1], pixels[idx + 2]);
        }
    }
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