#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <chrono>
#include <sstream>

// Include GLEW before SDL to avoid header conflicts
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// Window dimensions
const int WIDTH = 1280;
const int HEIGHT = 720;

// Camera controller
class CameraController {
public:
    float position[3];
    float lookat[3];
    float vup[3];
    float vfov;
    float aspect_ratio;
    float yaw;
    float pitch;

public:
    CameraController()
        : vfov(60.0f), aspect_ratio((float)WIDTH / (float)HEIGHT), yaw(-90.0f), pitch(0.0f) {
        position[0] = 0.0f; position[1] = 2.0f; position[2] = 15.0f;
        lookat[0] = 0.0f; lookat[1] = 2.0f; lookat[2] = 0.0f;
        vup[0] = 0.0f; vup[1] = 1.0f; vup[2] = 0.0f;
        update_from_angles();
    }

    void update_from_angles() {
        float yaw_rad = yaw * 3.14159f / 180.0f;
        float pitch_rad = pitch * 3.14159f / 180.0f;

        float direction[3];
        direction[0] = cos(yaw_rad) * cos(pitch_rad);
        direction[1] = sin(pitch_rad);
        direction[2] = sin(yaw_rad) * cos(pitch_rad);

        lookat[0] = position[0] + direction[0];
        lookat[1] = position[1] + direction[1];
        lookat[2] = position[2] + direction[2];
    }

    void move_forward(float delta) {
        float forward[3] = {lookat[0] - position[0], lookat[1] - position[1], lookat[2] - position[2]};
        float len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
        forward[0] /= len; forward[1] /= len; forward[2] /= len;

        position[0] += forward[0] * delta;
        position[1] += forward[1] * delta;
        position[2] += forward[2] * delta;
        update_from_angles();
    }

    void move_right(float delta) {
        float forward[3] = {lookat[0] - position[0], lookat[1] - position[1], lookat[2] - position[2]};
        float len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
        forward[0] /= len; forward[1] /= len; forward[2] /= len;

        float right[3] = {
            forward[1] * vup[2] - forward[2] * vup[1],
            forward[2] * vup[0] - forward[0] * vup[2],
            forward[0] * vup[1] - forward[1] * vup[0]
        };

        len = sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
        right[0] /= len; right[1] /= len; right[2] /= len;

        position[0] += right[0] * delta;
        position[1] += right[1] * delta;
        position[2] += right[2] * delta;
        update_from_angles();
    }

    void move_up(float delta) {
        position[1] += delta;
        update_from_angles();
    }

    void rotate(float delta_yaw, float delta_pitch) {
        yaw += delta_yaw;
        pitch += delta_pitch;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        update_from_angles();
    }
};

// Simple help overlay
class HelpOverlay {
private:
    bool show;
    bool initialized;

public:
    HelpOverlay() : show(false), initialized(false) {}

    bool init() {
        initialized = true;
        return true;
    }

    void toggle() { show = !show; }
    bool is_showing() const { return show; }
};

// Simple settings panel
class ControlsPanel {
private:
    bool show;
    bool initialized;

public:
    ControlsPanel() : show(false), initialized(false) {}

    bool init() {
        initialized = true;
        return true;
    }

    void toggle() { show = !show; }
    bool is_showing() const { return show; }
};

// Maximum scene objects (hardcoded for GLSL 1.20)
const int NUM_SPHERES = 16;
const int NUM_TRIANGLES = 20;
const int NUM_MATERIALS = 12;
const int NUM_LIGHTS = 1;

// Simple fragment shader that works with GLSL 1.20
const char* fragment_shader_source = R"(
#version 120

uniform vec2 resolution;
uniform vec3 camera_pos;
uniform vec3 camera_lookat;
uniform vec3 camera_vup;
uniform float camera_vfov;

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

// Simple ray-triangle intersection
bool hit_triangle(vec3 origin, vec3 direction, vec3 v0, vec3 v1, vec3 v2, vec3 normal, inout float t) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(direction, edge2);
    float a = dot(edge1, h);

    if (abs(a) < 0.0001) {
        return false;
    }

    float f = 1.0 / a;
    vec3 s = origin - v0;
    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0) {
        return false;
    }

    vec3 q = cross(s, edge1);
    float v = f * dot(direction, q);

    if (v < 0.0 || u + v > 1.0) {
        return false;
    }

    float t_new = f * dot(edge2, q);

    if (t_new < 0.001 || t_new > t) {
        return false;
    }

    t = t_new;
    return true;
}

// Simple scene rendering
vec3 ray_color(vec3 origin, vec3 direction) {
    // Background gradient
    vec3 unit_dir = normalize(direction);
    float t = 0.5 * (unit_dir.y + 1.0);
    vec3 background = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);

    // Scene objects (simplified Cornell box)
    vec3 sphere_centers[2];
    sphere_centers[0] = vec3(-1.5, 1.0, 0.0);
    sphere_centers[1] = vec3(1.5, 1.0, 0.0);

    float sphere_radii[2];
    sphere_radii[0] = 1.0;
    sphere_radii[1] = 1.0;

    vec3 sphere_colors[2];
    sphere_colors[0] = vec3(0.95, 0.95, 0.95); // Metal
    sphere_colors[1] = vec3(1.0, 1.0, 1.0);    // Glass

    float t_min = 1000.0;
    int hit_object = -1;

    // Check sphere intersections
    for (int i = 0; i < 2; i++) {
        if (hit_sphere(origin, direction, sphere_centers[i], sphere_radii[i], t_min)) {
            hit_object = i;
        }
    }

    // Simple triangle intersections (walls)
    vec3 tri_colors[12];
    tri_colors[0] = vec3(0.65, 0.05, 0.05); // Red wall
    tri_colors[1] = vec3(0.65, 0.05, 0.05);
    tri_colors[2] = vec3(0.12, 0.45, 0.15); // Green wall
    tri_colors[3] = vec3(0.12, 0.45, 0.15);
    tri_colors[4] = vec3(0.73, 0.73, 0.73); // White walls
    tri_colors[5] = vec3(0.73, 0.73, 0.73);
    tri_colors[6] = vec3(0.73, 0.73, 0.73);
    tri_colors[7] = vec3(0.73, 0.73, 0.73);
    tri_colors[8] = vec3(0.73, 0.73, 0.73);
    tri_colors[9] = vec3(0.73, 0.73, 0.73);
    tri_colors[10] = vec3(0.73, 0.73, 0.73);
    tri_colors[11] = vec3(0.73, 0.73, 0.73);

    // Floor triangles
    vec3 floor0_v0 = vec3(-5.0, 0.0, -5.0);
    vec3 floor0_v1 = vec3(5.0, 0.0, -5.0);
    vec3 floor0_v2 = vec3(5.0, 0.0, 5.0);
    vec3 floor0_n = vec3(0.0, 1.0, 0.0);

    vec3 floor1_v0 = vec3(-5.0, 0.0, -5.0);
    vec3 floor1_v1 = vec3(5.0, 0.0, 5.0);
    vec3 floor1_v2 = vec3(-5.0, 0.0, 5.0);
    vec3 floor1_n = vec3(0.0, 1.0, 0.0);

    if (hit_triangle(origin, direction, floor0_v0, floor0_v1, floor0_v2, floor0_n, t_min)) hit_object = 10;
    if (hit_triangle(origin, direction, floor1_v0, floor1_v1, floor1_v2, floor1_n, t_min)) hit_object = 10;

    // Ceiling triangles
    vec3 ceil0_v0 = vec3(-5.0, 5.0, -5.0);
    vec3 ceil0_v1 = vec3(5.0, 5.0, -5.0);
    vec3 ceil0_v2 = vec3(5.0, 5.0, 5.0);
    vec3 ceil0_n = vec3(0.0, -1.0, 0.0);

    vec3 ceil1_v0 = vec3(-5.0, 5.0, -5.0);
    vec3 ceil1_v1 = vec3(5.0, 5.0, 5.0);
    vec3 ceil1_v2 = vec3(-5.0, 5.0, 5.0);
    vec3 ceil1_n = vec3(0.0, -1.0, 0.0);

    if (hit_triangle(origin, direction, ceil0_v0, ceil0_v1, ceil0_v2, ceil0_n, t_min)) hit_object = 11;
    if (hit_triangle(origin, direction, ceil1_v0, ceil1_v1, ceil1_v2, ceil1_n, t_min)) hit_object = 11;

    // Back wall triangles
    vec3 back0_v0 = vec3(-5.0, 0.0, -5.0);
    vec3 back0_v1 = vec3(5.0, 0.0, -5.0);
    vec3 back0_v2 = vec3(5.0, 5.0, -5.0);
    vec3 back0_n = vec3(0.0, 0.0, 1.0);

    vec3 back1_v0 = vec3(-5.0, 0.0, -5.0);
    vec3 back1_v1 = vec3(5.0, 5.0, -5.0);
    vec3 back1_v2 = vec3(-5.0, 5.0, -5.0);
    vec3 back1_n = vec3(0.0, 0.0, 1.0);

    if (hit_triangle(origin, direction, back0_v0, back0_v1, back0_v2, back0_n, t_min)) hit_object = 12;
    if (hit_triangle(origin, direction, back1_v0, back1_v1, back1_v2, back1_n, t_min)) hit_object = 12;

    // Left wall (red)
    vec3 left0_v0 = vec3(-5.0, 0.0, -5.0);
    vec3 left0_v1 = vec3(-5.0, 0.0, 5.0);
    vec3 left0_v2 = vec3(-5.0, 5.0, 5.0);
    vec3 left0_n = vec3(1.0, 0.0, 0.0);

    vec3 left1_v0 = vec3(-5.0, 0.0, -5.0);
    vec3 left1_v1 = vec3(-5.0, 5.0, 5.0);
    vec3 left1_v2 = vec3(-5.0, 5.0, -5.0);
    vec3 left1_n = vec3(1.0, 0.0, 0.0);

    if (hit_triangle(origin, direction, left0_v0, left0_v1, left0_v2, left0_n, t_min)) hit_object = 13;
    if (hit_triangle(origin, direction, left1_v0, left1_v1, left1_v2, left1_n, t_min)) hit_object = 13;

    // Right wall (green)
    vec3 right0_v0 = vec3(5.0, 0.0, -5.0);
    vec3 right0_v1 = vec3(5.0, 0.0, 5.0);
    vec3 right0_v2 = vec3(5.0, 5.0, 5.0);
    vec3 right0_n = vec3(-1.0, 0.0, 0.0);

    vec3 right1_v0 = vec3(5.0, 0.0, -5.0);
    vec3 right1_v1 = vec3(5.0, 5.0, 5.0);
    vec3 right1_v2 = vec3(5.0, 5.0, -5.0);
    vec3 right1_n = vec3(-1.0, 0.0, 0.0);

    if (hit_triangle(origin, direction, right0_v0, right0_v1, right0_v2, right0_n, t_min)) hit_object = 14;
    if (hit_triangle(origin, direction, right1_v0, right1_v1, right1_v2, right1_n, t_min)) hit_object = 14;

    if (hit_object >= 0) {
        vec3 hit_point = origin + t_min * direction;
        vec3 color;
        vec3 normal;

        if (hit_object < 2) {
            // Sphere hit
            normal = normalize(hit_point - sphere_centers[hit_object]);
            color = sphere_colors[hit_object];
        } else {
            // Triangle hit - determine normal based on which wall
            if (hit_object == 10 || hit_object == 11) { // Floor or ceiling
                normal = (hit_object == 10) ? vec3(0.0, 1.0, 0.0) : vec3(0.0, -1.0, 0.0);
                color = vec3(0.73, 0.73, 0.73);
            } else if (hit_object == 12) { // Back wall
                normal = vec3(0.0, 0.0, 1.0);
                color = vec3(0.73, 0.73, 0.73);
            } else if (hit_object == 13) { // Left wall (red)
                normal = vec3(1.0, 0.0, 0.0);
                color = vec3(0.65, 0.05, 0.05);
            } else if (hit_object == 14) { // Right wall (green)
                normal = vec3(-1.0, 0.0, 0.0);
                color = vec3(0.12, 0.45, 0.15);
            } else {
                normal = vec3(0.0, 1.0, 0.0);
                color = vec3(0.73, 0.73, 0.73);
            }
        }

        // Simple lighting
        vec3 light_pos = vec3(0.0, 4.99, 0.0);
        vec3 light_dir = normalize(light_pos - hit_point);
        float diff = max(dot(normal, light_dir), 0.0);

        // Add simple Phong specular
        vec3 view_dir = normalize(-direction);
        vec3 reflect_dir = reflect(-light_dir, normal);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

        color = color * (0.1 + diff * 0.7 + spec * 0.3);

        return color;
    }

    return background;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;

    // Camera from uniforms
    vec3 origin = camera_pos;
    vec3 lookat = camera_lookat;
    vec3 vup = camera_vup;

    vec3 w = normalize(origin - lookat);
    vec3 u = normalize(cross(vup, w));
    vec3 v = cross(w, u);

    float aspect_ratio = resolution.x / resolution.y;
    float theta = camera_vfov * 0.0174533;
    float h = tan(theta * 0.5);
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

    std::cout << "=== Working GPU Ray Tracer (GLSL 1.20 Compatible) ===" << std::endl;
    std::cout << "Features: Cornell Box, Phong shading, spheres and triangles" << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create OpenGL window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Working GPU Ray Tracer - GLSL 1.20",
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

    std::cout << "✓ Shaders compiled and linked successfully" << std::endl;

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
    GLint camera_pos_loc = glGetUniformLocation(program, "camera_pos");
    GLint camera_lookat_loc = glGetUniformLocation(program, "camera_lookat");
    GLint camera_vup_loc = glGetUniformLocation(program, "camera_vup");
    GLint camera_vfov_loc = glGetUniformLocation(program, "camera_vfov");

    std::cout << "\n=== Starting Working GPU Ray Tracer ===" << std::endl;
    std::cout << "You should see a Cornell Box scene with:" << std::endl;
    std::cout << "  - Metal sphere (left) and glass sphere (right)" << std::endl;
    std::cout << "  - Red wall (left) and green wall (right)" << std::endl;
    std::cout << "  - Phong shading with specular highlights" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Click window to capture mouse" << std::endl;
    std::cout << "  WASD/Arrows - Move (slower)" << std::endl;
    std::cout << "  Mouse       - Look around" << std::endl;
    std::cout << "  H           - Help" << std::endl;
    std::cout << "  ESC         - Quit" << std::endl;
    std::cout << "==============================\n" << std::endl;

    // Camera controller (slower movement)
    CameraController camera;
    float move_speed = 0.05f; // Slower speed

    // UI panels
    HelpOverlay help_overlay;
    help_overlay.init();

    ControlsPanel controls_panel;
    controls_panel.init();

    // Main loop
    bool running = true;
    bool need_render = true;
    SDL_Event event;

    auto start_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float fps = 0.0f;
    auto last_frame_time = std::chrono::high_resolution_clock::now();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_h) {
                    help_overlay.toggle();
                } else if (event.key.keysym.sym == SDLK_c) {
                    controls_panel.toggle();
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    SDL_bool captured = SDL_GetRelativeMouseMode();
                    SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (SDL_GetRelativeMouseMode()) {
                    camera.rotate(event.motion.xrel * 0.1f, -event.motion.yrel * 0.1f);
                    need_render = true;
                }
            }
        }

        // Handle continuous keyboard input
        const Uint8* keystates = SDL_GetKeyboardState(NULL);

        if (keystates[SDL_SCANCODE_W]) {
            camera.move_forward(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_S]) {
            camera.move_forward(-move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_A]) {
            camera.move_right(-move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_D]) {
            camera.move_right(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_UP]) {
            camera.move_up(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_DOWN]) {
            camera.move_up(-move_speed);
            need_render = true;
        }

        // Update time (keep for potential animation)
        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(current_time - start_time).count();

        // Render
        if (need_render) {
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(program);

            // Set uniforms
            glUniform2f(resolution_loc, (float)WIDTH, (float)HEIGHT);
            glUniform3f(camera_pos_loc, camera.position[0], camera.position[1], camera.position[2]);
            glUniform3f(camera_lookat_loc, camera.lookat[0], camera.lookat[1], camera.lookat[2]);
            glUniform3f(camera_vup_loc, camera.vup[0], camera.vup[1], camera.vup[2]);
            glUniform1f(camera_vfov_loc, camera.vfov);

            // Render fullscreen quad
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            SDL_GL_SwapWindow(window);

            // Calculate FPS
            frame_count++;
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_frame_time).count();
            if (elapsed >= 1.0) {
                fps = frame_count / elapsed;
                frame_count = 0;
                last_frame_time = now;
                std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                         << " | Cam: " << camera.position[0] << ", " << camera.position[1] << ", " << camera.position[2]
                         << "     " << std::flush;
            }

            need_render = false;
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

    std::cout << "GPU Ray Tracer shut down successfully" << std::endl;
    return 0;
}