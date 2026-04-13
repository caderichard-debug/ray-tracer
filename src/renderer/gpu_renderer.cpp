#include "gpu_renderer.h"
#include "shader_manager.h"
#include <iostream>
#include <chrono>
#include <cstring>

// Quad vertices for screen-filling triangle
static const float quad_vertices[] = {
    -1.0f, -1.0f,
     3.0f, -1.0f,
    -1.0f,  3.0f
};

GPURenderer::GPURenderer()
    : compute_program(0)
    , display_program(0)
    , output_texture(0)
    , vao(0)
    , vbo(0)
    , sphere_ssbo(0)
    , triangle_ssbo(0)
    , material_ssbo(0)
    , light_ssbo(0)
    , camera_ubo(0)
    , num_spheres(0)
    , num_triangles(0)
    , num_lights(0)
    , width(0)
    , height(0)
    , last_render_time(0.0f)
    , last_ray_count(0)
    , perf_tracker("GPU", 1, 1, 1, 1)
    , initialized(false)
    , use_compute_shader(true)
    , sdl_window(nullptr)
    , gl_context(nullptr)
    , owns_context(false)
    , fragment_program(0)
    , vao_fallback(0)
    , vbo_fallback(0)
{
}

GPURenderer::~GPURenderer() {
    cleanup();
}

bool GPURenderer::initialize(SDL_Window* window, int width, int height) {
    if (initialized) {
        std::cerr << "GPURenderer already initialized" << std::endl;
        return false;
    }

    this->sdl_window = window;
    this->width = width;
    this->height = height;

    if (!init_opengl()) {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        return false;
    }

    if (!create_shaders()) {
        std::cerr << "Failed to create shaders" << std::endl;
        return false;
    }

    if (!create_buffers(width, height)) {
        std::cerr << "Failed to create buffers" << std::endl;
        return false;
    }

    if (!create_display_quad()) {
        std::cerr << "Failed to create display quad" << std::endl;
        return false;
    }

    initialized = true;
    return true;
}

bool GPURenderer::init_opengl() {
    // If window already has a context, use it
    gl_context = SDL_GL_GetCurrentContext();
    if (gl_context) {
        std::cout << "Using existing OpenGL context" << std::endl;
        owns_context = false;
    } else {
        // Create a new context for the window
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);  // macOS supports up to 4.1
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        gl_context = SDL_GL_CreateContext(sdl_window);
        if (!gl_context) {
            std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            return false;
        }
        owns_context = true;
    }

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return false;
    }

    std::cout << "OpenGL Context Initialized:" << std::endl;
    std::cout << "  Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "  Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "  GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // Set up OpenGL state for rendering
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Check for compute shader support
    GLint num_extensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    bool has_compute = false;
    for (int i = 0; i < num_extensions; i++) {
        const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (strcmp(ext, "GL_ARB_compute_shader") == 0) {
            has_compute = true;
            break;
        }
    }

    if (!has_compute) {
        std::cerr << "Warning: Compute shaders not supported, using fragment shader fallback" << std::endl;
    } else {
        std::cout << "  Compute Shaders: Supported" << std::endl;
    }

    return true;
}

bool GPURenderer::create_shaders() {
    ShaderManager sm;

    // Try compute shader first
    GLuint compute_shader = sm.load_compute_shader("raytrace.comp");
    if (compute_shader) {
        compute_program = sm.create_compute_program(compute_shader);
        if (compute_program) {
            sm.delete_shader(compute_shader);
            std::cout << "Using compute shader ray tracing" << std::endl;
            use_compute_shader = true;
            return create_display_shaders(sm);
        }
    }

    std::cout << "Compute shaders not available, falling back to fragment shader ray tracing" << std::endl;
    use_compute_shader = false;

    // Fall back to fragment shader approach
    GLuint vertex_shader = sm.load_vertex_shader("quad.vert");
    if (!vertex_shader) {
        std::cerr << "Failed to load vertex shader" << std::endl;
        return false;
    }

    GLuint fragment_shader = sm.load_fragment_shader("raytrace.frag");
    if (!fragment_shader) {
        std::cerr << "Failed to load fragment shader" << std::endl;
        sm.delete_shader(vertex_shader);
        return false;
    }

    fragment_program = sm.create_program(vertex_shader, fragment_shader);
    sm.delete_shader(vertex_shader);
    sm.delete_shader(fragment_shader);

    if (!fragment_program) {
        std::cerr << "Failed to create fragment program" << std::endl;
        return false;
    }

    std::cout << "Fragment shader ray tracing initialized" << std::endl;
    return true;
}

bool GPURenderer::create_display_shaders(ShaderManager& sm) {
    // Create display shader program (for compute shader output)
    GLuint vertex_shader = sm.load_vertex_shader("quad.vert");
    if (!vertex_shader) {
        std::cerr << "Failed to load vertex shader" << std::endl;
        return false;
    }

    GLuint fragment_shader = sm.load_fragment_shader("quad.frag");
    if (!fragment_shader) {
        std::cerr << "Failed to load fragment shader" << std::endl;
        sm.delete_shader(vertex_shader);
        return false;
    }

    display_program = sm.create_program(vertex_shader, fragment_shader);
    sm.delete_shader(vertex_shader);
    sm.delete_shader(fragment_shader);

    if (!display_program) {
        std::cerr << "Failed to create display program" << std::endl;
        return false;
    }

    return true;
}

bool GPURenderer::create_buffers(int width, int height) {
    // Create output texture (RGBA32F for HDR)
    glGenTextures(1, &output_texture);
    glBindTexture(GL_TEXTURE_2D, output_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindImageTexture(0, output_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // Create SSBOs for scene data (will be populated in set_scene)
    glGenBuffers(1, &sphere_ssbo);
    glGenBuffers(1, &triangle_ssbo);
    glGenBuffers(1, &material_ssbo);
    glGenBuffers(1, &light_ssbo);
    glGenBuffers(1, &camera_ubo);

    std::cout << "Buffers created successfully" << std::endl;
    return true;
}

bool GPURenderer::create_display_quad() {
    // Create VAO and VBO for screen quad
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void GPURenderer::set_scene(const Scene& scene) {
    upload_scene_data(scene);
}

void GPURenderer::upload_scene_data(const Scene& scene) {
    (void)scene; // TODO: Extract proper scene data

    // Count scene elements
    num_spheres = 0;
    num_triangles = 0;

    for (const auto& obj : scene.objects) {
        (void)obj; // TODO: Extract primitive type and count
        num_spheres++;
    }

    num_lights = scene.lights.size();

    // Prepare scene data for upload
    std::vector<float> sphere_data;
    sphere_data.reserve(num_spheres * 4); // vec4 per sphere

    // Extract sphere data from scene
    for (const auto& obj : scene.objects) {
        // Simplified: assuming all objects are spheres for demo
        // In real implementation, check type and extract accordingly
        sphere_data.push_back(0.0f); // x
        sphere_data.push_back(0.0f); // y
        sphere_data.push_back(0.0f); // z
        sphere_data.push_back(0.5f); // radius
    }

    // Create Cornell box spheres for demo
    sphere_data.clear();
    sphere_data.reserve(20 * 4);

    // Center sphere
    sphere_data.insert(sphere_data.end(), {0.0f, 0.0f, 0.0f, 0.5f});
    // Red sphere
    sphere_data.insert(sphere_data.end(), {-1.2f, 0.2f, -0.5f, 0.35f});
    // Blue sphere
    sphere_data.insert(sphere_data.end(), {1.2f, -0.1f, -0.8f, 0.4f});
    // Small sphere
    sphere_data.insert(sphere_data.end(), {0.0f, -0.5f, 0.5f, 0.25f});
    // Yellow sphere
    sphere_data.insert(sphere_data.end(), {-0.6f, -0.4f, 0.8f, 0.2f});

    num_spheres = sphere_data.size() / 4;

    // Upload sphere data as texture buffer (for fragment shader compatibility)
    glGenBuffers(1, &sphere_ssbo);
    glBindBuffer(GL_TEXTURE_BUFFER, sphere_ssbo);
    glBufferData(GL_TEXTURE_BUFFER, sphere_data.size() * sizeof(float), sphere_data.data(), GL_STATIC_DRAW);

    // Create texture buffer texture
    GLuint sphere_texture;
    glGenTextures(1, &sphere_texture);
    glBindTexture(GL_TEXTURE_BUFFER, sphere_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, sphere_ssbo);

    // Upload triangle data (empty for now)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangle_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);

    // Upload material data (empty for now)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, material_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);

    // Upload light data (empty for now)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);

    std::cout << "Scene data uploaded: " << num_spheres << " spheres" << std::endl;
}

void GPURenderer::upload_camera_data(const Camera& camera) {
    // Camera uniform block
    struct CameraData {
        float position[4];
        float lookat[4];
        float vup[4];
        float vfov;
        float aspect_ratio;
        float aperture;
        float dist_to_focus;
        float padding_;
    };

    CameraData cam_data;

    // Copy camera position
    cam_data.position[0] = camera.lookfrom.x;
    cam_data.position[1] = camera.lookfrom.y;
    cam_data.position[2] = camera.lookfrom.z;
    cam_data.position[3] = 0.0f;

    // Copy camera look target
    cam_data.lookat[0] = camera.lookat.x;
    cam_data.lookat[1] = camera.lookat.y;
    cam_data.lookat[2] = camera.lookat.z;
    cam_data.lookat[3] = 0.0f;

    // Copy camera up vector
    cam_data.vup[0] = camera.vup.x;
    cam_data.vup[1] = camera.vup.y;
    cam_data.vup[2] = camera.vup.z;
    cam_data.vup[3] = 0.0f;

    cam_data.vfov = camera.vfov;
    cam_data.aspect_ratio = camera.aspect_ratio;
    cam_data.aperture = camera.aperture;
    cam_data.dist_to_focus = camera.focus_dist;
    cam_data.padding_ = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, camera_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraData), &cam_data, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 4, camera_ubo);
}

void GPURenderer::dispatch_compute(int samples_per_pixel, int max_depth) {
    glUseProgram(compute_program);

    // Set uniforms
    glUniform2i(glGetUniformLocation(compute_program, "resolution"), width, height);
    glUniform1i(glGetUniformLocation(compute_program, "num_spheres"), num_spheres);
    glUniform1i(glGetUniformLocation(compute_program, "num_triangles"), num_triangles);
    glUniform1i(glGetUniformLocation(compute_program, "num_lights"), num_lights);
    glUniform1i(glGetUniformLocation(compute_program, "max_depth"), max_depth);
    glUniform1i(glGetUniformLocation(compute_program, "samples_per_pixel"), samples_per_pixel);

    // Set ambient light
    glUniform3f(glGetUniformLocation(compute_program, "ambient_light"), 0.1f, 0.1f, 0.1f);

    // Bind output texture
    glBindImageTexture(0, output_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindTextureUnit(0, output_texture);

    // Dispatch compute shader
    int workgroup_size = 16;
    int num_groups_x = (width + workgroup_size - 1) / workgroup_size;
    int num_groups_y = (height + workgroup_size - 1) / workgroup_size;

    glDispatchCompute(num_groups_x, num_groups_y, 1);

    // Wait for compute to finish
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    glUseProgram(0);
}

void GPURenderer::render(const Camera& camera, const Scene& scene, int samples_per_pixel, int max_depth) {
    (void)scene; // Scene data already uploaded via set_scene()
    (void)samples_per_pixel; // Handled by shader

    if (!initialized) {
        std::cerr << "GPURenderer not initialized" << std::endl;
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Upload camera data
    upload_camera_data(camera);

    if (use_compute_shader) {
        // Dispatch compute shader (original path)
        dispatch_compute(samples_per_pixel, max_depth);
    } else {
        // Render with fragment shader
        render_fragment(camera, max_depth);
    }

    auto end = std::chrono::high_resolution_clock::now();
    last_render_time = std::chrono::duration<float>(end - start).count();

    // Calculate ray count
    last_ray_count = (uint64_t)width * height * samples_per_pixel;
}

void GPURenderer::render_fragment(const Camera& camera, int max_depth) {
    (void)camera; // Camera data is already uploaded via uniform buffer
    (void)max_depth;

    // Clear screen with a visible color to test
    glClearColor(0.8f, 0.3f, 0.2f, 1.0f);  // Red/orange clear color
    glClear(GL_COLOR_BUFFER_BIT);

    // Set viewport
    glViewport(0, 0, width, height);

    // Use fragment program
    glUseProgram(fragment_program);
    if (fragment_program == 0) {
        std::cerr << "Fragment program is 0, not using it" << std::endl;
        return;
    }

    // Set resolution uniform
    GLint res_loc = glGetUniformLocation(fragment_program, "resolution");
    if (res_loc >= 0) {
        glUniform2i(res_loc, width, height);
    }

    // Draw full-screen quad
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glUseProgram(0);

    // Check for OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error during render: " << err << std::endl;
    }
}

void GPURenderer::get_framebuffer(unsigned char* data, int width, int height) {
    (void)width;  // Use class member width instead
    (void)height; // Use class member height instead

    // Read back texture data
    glBindTexture(GL_TEXTURE_2D, output_texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GPURenderer::display() {
    if (use_compute_shader) {
        // Display compute shader output
        glUseProgram(display_program);

        // Bind output texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, output_texture);
        glUniform1i(glGetUniformLocation(display_program, "u_texture"), 0);

        // Draw quad
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glUseProgram(0);
    }
    // Fragment shader renders directly to screen, no display step needed
}

void GPURenderer::cleanup() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vao_fallback) glDeleteVertexArrays(1, &vao_fallback);
    if (vbo_fallback) glDeleteBuffers(1, &vbo_fallback);
    if (output_texture) glDeleteTextures(1, &output_texture);
    if (sphere_ssbo) glDeleteBuffers(1, &sphere_ssbo);
    if (triangle_ssbo) glDeleteBuffers(1, &triangle_ssbo);
    if (material_ssbo) glDeleteBuffers(1, &material_ssbo);
    if (light_ssbo) glDeleteBuffers(1, &light_ssbo);
    if (camera_ubo) glDeleteBuffers(1, &camera_ubo);
    if (compute_program) glDeleteProgram(compute_program);
    if (display_program) glDeleteProgram(display_program);
    if (fragment_program) glDeleteProgram(fragment_program);

    if (gl_context && owns_context) {
        SDL_GL_DeleteContext(gl_context);
        gl_context = nullptr;
    }

    // Don't destroy the window - it's owned by the main application
    sdl_window = nullptr;

    initialized = false;
}
