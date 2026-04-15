#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <chrono>

// Include GLEW before SDL to avoid header conflicts
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// Window dimensions
const int WIDTH = 1280;
const int HEIGHT = 720;

// Camera controller (same as CPU version)
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

        // Cross product with vup
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

// Fragment shader matching CPU renderer scene
const char* fragment_shader_source = R"(
#version 120

uniform vec2 resolution;
uniform vec3 camera_pos;
uniform vec3 camera_lookat;
uniform vec3 camera_vup;
uniform float camera_vfov;

// Ray-sphere intersection
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

// Ray-triangle intersection
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

// Scene rendering (matching CPU Cornell Box)
vec3 ray_color(vec3 origin, vec3 direction) {
    // Background gradient
    vec3 unit_dir = normalize(direction);
    float t_bg = 0.5 * (unit_dir.y + 1.0);
    vec3 background = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t_bg);

    float t_min = 1000.0;
    int hit_object = -1;
    vec3 hit_point;
    vec3 hit_normal;

    // === WALLS (large spheres like CPU version) ===

    // Back wall (green)
    if (hit_sphere(origin, direction, vec3(0, 0, -20.0), 16.0, t_min)) {
        hit_object = 0; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(0, 0, -20.0));
    }
    // Floor (gray)
    if (hit_sphere(origin, direction, vec3(0, -20.0, 0), 16.0, t_min)) {
        hit_object = 1; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(0, -20.0, 0));
    }
    // Ceiling (gray)
    if (hit_sphere(origin, direction, vec3(0, 20.0, 0), 16.0, t_min)) {
        hit_object = 2; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(0, 20.0, 0));
    }
    // Left wall (red)
    if (hit_sphere(origin, direction, vec3(-20.0, 0, 0), 16.0, t_min)) {
        hit_object = 3; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-20.0, 0, 0));
    }
    // Right wall (green - note: CPU has green here too)
    if (hit_sphere(origin, direction, vec3(20.0, 0, 0), 16.0, t_min)) {
        hit_object = 4; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(20.0, 0, 0));
    }

    // === OBJECTS (matching CPU version) ===

    // Center sphere (gold)
    if (hit_sphere(origin, direction, vec3(0, 0, 0), 2.0, t_min)) {
        hit_object = 10; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(0, 0, 0));
    }
    // Orbiting spheres
    if (hit_sphere(origin, direction, vec3(-3.0, 1.0, 2.0), 0.8, t_min)) {
        hit_object = 11; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-3.0, 1.0, 2.0));
    }
    if (hit_sphere(origin, direction, vec3(3.0, 0.8, 2.0), 0.9, t_min)) {
        hit_object = 12; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(3.0, 0.8, 2.0));
    }
    if (hit_sphere(origin, direction, vec3(0, 0.5, 1.5), 0.6, t_min)) {
        hit_object = 13; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(0, 0.5, 1.5));
    }
    if (hit_sphere(origin, direction, vec3(-1.5, 0.3, 2.0), 0.5, t_min)) {
        hit_object = 14; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-1.5, 0.3, 2.0));
    }
    // Glass sphere
    if (hit_sphere(origin, direction, vec3(1.0, -1.5, 2.5), 0.8, t_min)) {
        hit_object = 15; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(1.0, -1.5, 2.5));
    }
    if (hit_sphere(origin, direction, vec3(0.5, -2.0, 1.5), 0.5, t_min)) {
        hit_object = 16; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(0.5, -2.0, 1.5));
    }
    // Small metal spheres
    if (hit_sphere(origin, direction, vec3(-0.5, 2.5, 0.0), 0.2, t_min)) {
        hit_object = 17; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-0.5, 2.5, 0.0));
    }
    if (hit_sphere(origin, direction, vec3(-3.5, 2.8, 0.0), 0.2, t_min)) {
        hit_object = 18; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-3.5, 2.8, 0.0));
    }
    // Checkerboard sphere
    if (hit_sphere(origin, direction, vec3(-3.0, 1.0, -2.0), 0.8, t_min)) {
        hit_object = 19; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-3.0, 1.0, -2.0));
    }
    // Noise sphere
    if (hit_sphere(origin, direction, vec3(-3.5, -2.0, 2.0), 0.8, t_min)) {
        hit_object = 20; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-3.5, -2.0, 2.0));
    }
    // Gradient sphere
    if (hit_sphere(origin, direction, vec3(-1.0, -1.5, 2.0), 0.8, t_min)) {
        hit_object = 21; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(-1.0, -1.5, 2.0));
    }
    // Stripe sphere
    if (hit_sphere(origin, direction, vec3(0.5, 1.5, 2.0), 0.8, t_min)) {
        hit_object = 22; hit_point = origin + t_min * direction;
        hit_normal = normalize(hit_point - vec3(0.5, 1.5, 2.0));
    }

    // === PYRAMID (4 triangles) ===
    vec3 pyramid_top = vec3(-2.0, 4.0, 0.0);
    vec3 pyramid_base1 = vec3(-1.0, 2.0, -1.0);
    vec3 pyramid_base2 = vec3(-3.0, 2.0, -1.0);
    vec3 pyramid_base3 = vec3(-2.0, 2.0, 1.0);

    if (hit_triangle(origin, direction, pyramid_top, pyramid_base1, pyramid_base2,
                    normalize(cross(pyramid_base1 - pyramid_top, pyramid_base2 - pyramid_top)), t_min)) {
        hit_object = 30; hit_point = origin + t_min * direction;
        hit_normal = normalize(cross(pyramid_base1 - pyramid_top, pyramid_base2 - pyramid_top));
    }
    if (hit_triangle(origin, direction, pyramid_top, pyramid_base2, pyramid_base3,
                    normalize(cross(pyramid_base2 - pyramid_top, pyramid_base3 - pyramid_top)), t_min)) {
        hit_object = 30; hit_point = origin + t_min * direction;
        hit_normal = normalize(cross(pyramid_base2 - pyramid_top, pyramid_base3 - pyramid_top));
    }
    if (hit_triangle(origin, direction, pyramid_top, pyramid_base3, pyramid_base1,
                    normalize(cross(pyramid_base3 - pyramid_top, pyramid_base1 - pyramid_top)), t_min)) {
        hit_object = 30; hit_point = origin + t_min * direction;
        hit_normal = normalize(cross(pyramid_base3 - pyramid_top, pyramid_base1 - pyramid_top));
    }
    if (hit_triangle(origin, direction, pyramid_base1, pyramid_base3, pyramid_base2,
                    normalize(cross(pyramid_base3 - pyramid_base1, pyramid_base2 - pyramid_base1)), t_min)) {
        hit_object = 30; hit_point = origin + t_min * direction;
        hit_normal = normalize(cross(pyramid_base3 - pyramid_base1, pyramid_base2 - pyramid_base1));
    }

    if (hit_object >= 0) {
        vec3 color;
        vec3 light_pos = vec3(0, 18.0, 0);

        // Determine material based on hit object
        if (hit_object == 0 || hit_object == 4) { // Green walls
            color = vec3(0.12, 0.45, 0.15);
        } else if (hit_object == 3) { // Red wall
            color = vec3(0.65, 0.05, 0.05);
        } else if (hit_object == 1 || hit_object == 2) { // Gray floor/ceiling
            color = vec3(0.73, 0.73, 0.73);
        } else if (hit_object == 10) { // Gold sphere
            color = vec3(1.0, 0.77, 0.35);
        } else if (hit_object == 11 || hit_object == 14) { // Fuzzy metal
            color = vec3(0.7, 0.6, 0.5);
        } else if (hit_object == 12) { // Blue sphere
            color = vec3(0.1, 0.2, 0.7);
        } else if (hit_object == 13) { // Red sphere
            color = vec3(0.65, 0.05, 0.05);
        } else if (hit_object == 15) { // Glass
            color = vec3(1.0, 1.0, 1.0);
        } else if (hit_object == 16) { // Red small sphere
            color = vec3(0.65, 0.05, 0.05);
        } else if (hit_object == 17 || hit_object == 18) { // Metal spheres
            color = vec3(0.8, 0.8, 0.8);
        } else if (hit_object == 30) { // Pyramid (checkerboard)
            float checker = mod(floor(hit_point.x) + floor(hit_point.y) + floor(hit_point.z), 2.0);
            color = checker > 0.5 ? vec3(0.9) : vec3(0.1);
        } else {
            color = vec3(0.73, 0.73, 0.73); // Default gray
        }

        // Simple lighting
        vec3 light_dir = normalize(light_pos - hit_point);
        float diff = max(dot(hit_normal, light_dir), 0.0);

        // Phong specular
        vec3 view_dir = normalize(-direction);
        vec3 reflect_dir = reflect(-light_dir, hit_normal);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

        color = color * (0.1 + diff * 0.7 + spec * 0.3);

        return color;
    }

    return background;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;

    // Camera setup
    vec3 origin = camera_pos;
    vec3 w = normalize(origin - camera_lookat);
    vec3 u = normalize(cross(camera_vup, w));
    vec3 v = cross(w, u);

    float theta = camera_vfov * 0.0174533;
    float h = tan(theta * 0.5);
    float viewport_height = 2.0 * h;
    float viewport_width = resolution.x / resolution.y * viewport_height;

    vec3 horizontal = viewport_width * u;
    vec3 vertical = viewport_height * v;
    vec3 lower_left_corner = origin - horizontal * 0.5 - vertical * 0.5 - w;

    vec3 direction = lower_left_corner + uv.x * horizontal + uv.y * vertical - origin;

    vec3 color = ray_color(origin, direction);

    gl_FragColor = vec4(color, 1.0);
}
)";

// Vertex shader
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
        std::cerr << "Failed to compile shader:" << std::endl << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

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
        std::cerr << "Failed to link program:" << std::endl << log.data() << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Interactive GPU Ray Tracer (Same Scene as CPU) ===" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Interactive GPU Ray Tracer - Same Scene as CPU",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    GLuint program = link_program(vertex_shader, fragment_shader);

    if (!program) {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "✓ Shaders compiled successfully" << std::endl;

    // Create fullscreen quad
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    float quad_vertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    GLint pos_loc = glGetAttribLocation(program, "position");
    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(pos_loc);

    GLint tex_loc = glGetAttribLocation(program, "texCoord");
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(tex_loc);

    // Get uniform locations
    GLint resolution_loc = glGetUniformLocation(program, "resolution");
    GLint camera_pos_loc = glGetUniformLocation(program, "camera_pos");
    GLint camera_lookat_loc = glGetUniformLocation(program, "camera_lookat");
    GLint camera_vup_loc = glGetUniformLocation(program, "camera_vup");
    GLint camera_vfov_loc = glGetUniformLocation(program, "camera_vfov");

    // Camera controller (same starting position as CPU)
    CameraController camera;

    std::cout << "\n=== Starting Interactive GPU Ray Tracer ===" << std::endl;
    std::cout << "Same scene as CPU renderer:" << std::endl;
    std::cout << "  - Cornell Box with colored walls" << std::endl;
    std::cout << "  - Gold sphere, glass sphere, orbiting spheres" << std::endl;
    std::cout << "  - Pyramid with procedural textures" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Click window to capture mouse for looking around" << std::endl;
    std::cout << "  WASD          - Move camera" << std::endl;
    std::cout << "  Arrow Keys    - Move up/down" << std::endl;
    std::cout << "  Mouse         - Look around (when captured)" << std::endl;
    std::cout << "  ESC           - Quit" << std::endl;
    std::cout << "==============================\n" << std::endl;

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
        float move_speed = 0.15f;

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

        // Render
        if (need_render) {
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(program);

            // Update camera uniforms
            glUniform2f(resolution_loc, (float)WIDTH, (float)HEIGHT);
            glUniform3f(camera_pos_loc, camera.position[0], camera.position[1], camera.position[2]);
            glUniform3f(camera_lookat_loc, camera.lookat[0], camera.lookat[1], camera.lookat[2]);
            glUniform3f(camera_vup_loc, camera.vup[0], camera.vup[1], camera.vup[2]);
            glUniform1f(camera_vfov_loc, camera.vfov);

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
                         << " | Pos: " << std::setprecision(1) << camera.position[0] << ", "
                         << camera.position[1] << ", " << camera.position[2] << "     " << std::flush;
            }

            need_render = false;
        }

        SDL_Delay(1);
    }

    std::cout << "\n=== Exiting ===" << std::endl;

    glDeleteProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Interactive GPU Ray Tracer shut down successfully" << std::endl;
    return 0;
}