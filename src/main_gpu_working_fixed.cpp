#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <chrono>
#include <sstream>

// Include GLEW before SDL
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_opengl.h>

const int WIDTH = 1280;
const int HEIGHT = 720;

struct QualityPreset {
    int samples;
    int max_depth;
    const char* name;
};

QualityPreset quality_levels[] = {
    {1, 1, "Preview"},
    {1, 3, "Low"},
    {4, 3, "Medium"},
    {16, 5, "High"},
    {32, 5, "Ultra"}
};

const int NUM_QUALITY_LEVELS = 5;

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

class HelpOverlay {
private:
    TTF_Font* font;
    TTF_Font* title_font;
    SDL_Color text_color;
    SDL_Color background_color;
    SDL_Color title_color;
    bool initialized;

public:
    HelpOverlay() : font(nullptr), title_font(nullptr), initialized(false) {
        text_color = {20, 20, 20, 255};
        background_color = {200, 200, 200, 180};
        title_color = {200, 50, 50, 255};
    }

    bool init() {
        if (initialized) return true;

        if (TTF_Init() == -1) {
            return false;
        }

        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/System/Library/Fonts/Menlo.ttc",
            nullptr
        };

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            font = TTF_OpenFont(font_paths[i], 13);
            if (font) break;
        }

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            title_font = TTF_OpenFont(font_paths[i], 18);
            if (title_font) break;
        }

        if (!font || !title_font) {
            return false;
        }

        initialized = true;
        return true;
    }

    void render(SDL_Renderer* renderer, int window_width, int window_height) {
        if (!initialized || !font || !title_font) return;

        SDL_Rect overlay_rect = {
            (window_width - 460) / 2,
            (window_height - 340) / 2,
            460, 340
        };

        SDL_Surface* surface = SDL_CreateRGBSurface(0, overlay_rect.w, overlay_rect.h, 32, 0, 0, 0, 0);
        if (!surface) return;

        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 200, 200, 200, 180));

        const char* title_text = "HELP";
        SDL_Surface* title_surface = TTF_RenderText_Blended(title_font, title_text, title_color);
        if (title_surface) {
            SDL_Rect title_rect = {(460 - title_surface->w) / 2, 12, title_surface->w, title_surface->h};
            SDL_BlitSurface(title_surface, nullptr, surface, &title_rect);
            SDL_FreeSurface(title_surface);
        }

        const char* controls_text[] = {
            "Click window to capture mouse for looking around",
            "MOVE: WASD + Arrows | LOOK: Mouse",
            "1-5: Quality | SPACE: Pause",
            "H: Help | C: Controls | ESC: Quit"
        };

        int y_offset = 50;
        for (size_t i = 0; i < sizeof(controls_text) / sizeof(controls_text[0]); ++i) {
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, controls_text[i], text_color);
            if (text_surface) {
                SDL_Rect text_rect = {(460 - text_surface->w) / 2, y_offset, text_surface->w, text_surface->h};
                SDL_BlitSurface(text_surface, nullptr, surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }
            y_offset += 40;
        }

        const char* footer = "Press H to close";
        SDL_Surface* footer_surface = TTF_RenderText_Blended(font, footer, text_color);
        if (footer_surface) {
            SDL_Rect footer_rect = {(460 - footer_surface->w) / 2, 280, footer_surface->w, footer_surface->h};
            SDL_BlitSurface(footer_surface, nullptr, surface, &footer_rect);
            SDL_FreeSurface(footer_surface);
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_RenderCopy(renderer, texture, nullptr, &overlay_rect);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
    }

    ~HelpOverlay() {
        if (font) TTF_CloseFont(font);
        if (title_font) TTF_CloseFont(title_font);
        if (initialized) TTF_Quit();
    }
};

class ControlsPanel {
private:
    TTF_Font* font;
    TTF_Font* title_font;
    SDL_Color text_color;
    SDL_Color background_color;
    SDL_Color title_color;
    SDL_Color value_color;
    bool initialized;

public:
    ControlsPanel() : font(nullptr), title_font(nullptr), initialized(false) {
        text_color = {20, 20, 20, 255};
        background_color = {50, 50, 60, 230};
        title_color = {100, 200, 255, 255};
        value_color = {255, 200, 100, 255};
    }

    bool init() {
        if (initialized) return true;

        if (TTF_Init() == -1) {
            return false;
        }

        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/System/Library/Fonts/Menlo.ttc",
            nullptr
        };

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            font = TTF_OpenFont(font_paths[i], 14);
            if (font) break;
        }

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            title_font = TTF_OpenFont(font_paths[i], 16);
            if (title_font) break;
        }

        if (!font || !title_font) {
            return false;
        }

        initialized = true;
        return true;
    }

    void render(SDL_Renderer* renderer, int window_width, int window_height,
                int quality_idx, const QualityPreset& preset, double fps, double render_time) {
        if (!initialized || !font || !title_font) return;

        int panel_width = std::min(300, window_width - 20);
        int panel_height = std::min(250, window_height - 20);
        int panel_x = window_width - panel_width - 10;
        int panel_y = 10;

        SDL_Rect overlay_rect = {panel_x, panel_y, panel_width, panel_height};

        SDL_Surface* surface = SDL_CreateRGBSurface(0, overlay_rect.w, overlay_rect.h, 32, 0, 0, 0, 0);
        if (!surface) return;

        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 50, 50, 60, 230));

        const char* title_text = "SETTINGS";
        SDL_Surface* title_surface = TTF_RenderText_Blended(title_font, title_text, title_color);
        if (title_surface) {
            SDL_Rect title_rect = {15, 10, title_surface->w, title_surface->h};
            SDL_BlitSurface(title_surface, nullptr, surface, &title_rect);
            SDL_FreeSurface(title_surface);
        }

        SDL_Rect separator = {10, 35, panel_width - 20, 1};
        SDL_FillRect(surface, &separator, SDL_MapRGBA(surface->format, 100, 100, 120, 255));

        int y_offset = 50;

        auto render_setting = [&](const char* label, const char* value) {
            SDL_Surface* label_surface = TTF_RenderText_Blended(font, label, text_color);
            if (label_surface) {
                SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
                SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
                SDL_FreeSurface(label_surface);
            }

            SDL_Surface* value_surface = TTF_RenderText_Blended(font, value, value_color);
            if (value_surface) {
                SDL_Rect value_rect = {panel_width - value_surface->w - 15, y_offset, value_surface->w, value_surface->h};
                SDL_BlitSurface(value_surface, nullptr, surface, &value_rect);
                SDL_FreeSurface(value_surface);
            }

            y_offset += 28;
        };

        char fps_str[32], time_str[32];
        snprintf(fps_str, sizeof(fps_str), "%.1f FPS", fps);
        snprintf(time_str, sizeof(time_str), "%.3f s", render_time);

        render_setting("Quality:", preset.name);
        render_setting("Samples:", std::to_string(preset.samples).c_str());
        render_setting("Depth:", std::to_string(preset.max_depth).c_str());
        render_setting("FPS:", fps_str);

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_RenderCopy(renderer, texture, nullptr, &overlay_rect);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
    }

    ~ControlsPanel() {
        if (font) TTF_CloseFont(font);
        if (title_font) TTF_CloseFont(title_font);
        if (initialized) TTF_Quit();
    }
};

// Simplified, working fragment shader
const char* fragment_shader_source = R"(
#version 120

uniform vec2 resolution;
uniform vec3 camera_pos;
uniform vec3 camera_lookat;
uniform vec3 camera_vup;
uniform float camera_vfov;
uniform int max_depth;

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

// Simple scene color
vec3 get_scene_color(vec3 hit_point, vec3 normal) {
    // Background gradient
    vec3 unit_dir = normalize(hit_point);
    float t = 0.5 * (unit_dir.y + 1.0);
    vec3 background = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);

    // Simple lighting
    vec3 light_pos = vec3(0, 18.0, 0);
    vec3 light_dir = normalize(light_pos - hit_point);
    float diff = max(dot(normal, light_dir), 0.0);

    // Phong specular
    vec3 view_dir = normalize(-hit_point); // Approximate
    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

    // Material colors based on position
    vec3 color;
    if (abs(hit_point.x) < 2.1 && abs(hit_point.y) < 2.1 && abs(hit_point.z) < 2.1) {
        color = vec3(1.0, 0.77, 0.35); // Gold sphere (center)
    } else if (hit_point.y > 1.0 && hit_point.y < 3.0) {
        if (hit_point.x < -2.0) {
            color = vec3(0.7, 0.6, 0.5); // Fuzzy metal
        } else if (hit_point.x > 2.0) {
            color = vec3(0.1, 0.2, 0.7); // Blue sphere
        } else {
            color = vec3(0.65, 0.05, 0.05); // Red sphere
        }
    } else if (hit_point.y < -1.0) {
        if (hit_point.x > 0.0) {
            color = vec3(1.0, 1.0, 1.0); // Glass sphere
        } else {
            color = vec3(0.65, 0.05, 0.05); // Red sphere
        }
    } else if (abs(hit_point.x + 3.0) < 1.0 && abs(hit_point.y) < 1.0) {
        float checker = mod(floor(hit_point.x * 2.0) + floor(hit_point.y * 2.0), 2.0);
        color = checker > 0.5 ? vec3(0.9) : vec3(0.1); // Checkerboard
    } else if (hit_point.y < -15.0) {
        color = vec3(0.73, 0.73, 0.73); // Floor
    } else if (hit_point.y > 15.0) {
        color = vec3(0.73, 0.73, 0.73); // Ceiling
    } else if (hit_point.x < -15.0) {
        color = vec3(0.65, 0.05, 0.05); // Left wall (red)
    } else if (hit_point.x > 15.0) {
        color = vec3(0.12, 0.45, 0.15); // Right wall (green)
    } else if (hit_point.z < -15.0) {
        color = vec3(0.12, 0.45, 0.15); // Back wall (green)
    } else {
        color = vec3(0.73, 0.73, 0.73); // Default gray
    }

    return color * (0.1 + diff * 0.7 + spec * 0.3);
}

vec3 ray_color(vec3 origin, vec3 direction) {
    float t_min = 1000.0;
    vec3 hit_point, hit_normal;
    bool hit = false;

    // Check all scene objects
    vec3 centers[11];
    float radii[11];

    // Walls
    centers[0] = vec3(0, 0, -20.0); radii[0] = 16.0; // Back
    centers[1] = vec3(0, -20.0, 0);   radii[1] = 16.0; // Floor
    centers[2] = vec3(0, 20.0, 0);    radii[2] = 16.0; // Ceiling
    centers[3] = vec3(-20.0, 0, 0);  radii[3] = 16.0; // Left
    centers[4] = vec3(20.0, 0, 0);   radii[4] = 16.0; // Right

    // Objects
    centers[5] = vec3(0, 0, 0);        radii[5] = 2.0;  // Center gold
    centers[6] = vec3(-3.0, 1.0, 2.0); radii[6] = 0.8;  // Fuzzy metal
    centers[7] = vec3(3.0, 0.8, 2.0);  radii[7] = 0.9;  // Blue
    centers[8] = vec3(0, 0.5, 1.5);     radii[8] = 0.6;  // Red
    centers[9] = vec3(-1.5, 0.3, 2.0); radii[9] = 0.5;  // Red
    centers[10] = vec3(1.0, -1.5, 2.5); radii[10] = 0.8; // Glass

    for (int i = 0; i < 11; i++) {
        if (hit_sphere(origin, direction, centers[i], radii[i], t_min)) {
            hit_point = origin + t_min * direction;
            hit_normal = normalize(hit_point - centers[i]);
            hit = true;
        }
    }

    if (hit) {
        return get_scene_color(hit_point, hit_normal);
    }

    // Background gradient
    vec3 unit_dir = normalize(direction);
    float t = 0.5 * (unit_dir.y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;

    // Camera setup (don't flip Y here, do it in vertex shader)
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

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    gl_FragColor = vec4(color, 1.0);
}
)";

const char* vertex_shader_source = R"(
#version 120

attribute vec2 position;
attribute vec2 texCoord;

varying vec2 uv;

void main() {
    uv = texCoord;
    // Flip Y here instead of in fragment shader
    uv.y = 1.0 - texCoord.y;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

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
    std::cout << "=== Interactive GPU Ray Tracer (Working Version) ===" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Interactive GPU Ray Tracer - Working",
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
    GLint max_depth_loc = glGetUniformLocation(program, "max_depth");

    // Camera controller (slower movement)
    CameraController camera;

    // Settings
    int current_quality = 1;
    QualityPreset preset = quality_levels[current_quality];

    // Initialize UI
    HelpOverlay help_overlay;
    if (help_overlay.init()) {
        std::cout << "✓ Help overlay initialized" << std::endl;
    }

    ControlsPanel controls_panel;
    if (controls_panel.init()) {
        std::cout << "✓ Controls panel initialized" << std::endl;
    }

    bool show_help = false;
    bool show_controls = false;

    std::cout << "\n=== Starting Interactive GPU Ray Tracer ===" << std::endl;
    std::cout << "Features: Cornell Box, Phong shading, slower camera" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Click window to capture mouse" << std::endl;
    std::cout << "  WASD/Arrows  - Move (slower)" << std::endl;
    std::cout << "  Mouse       - Look around" << std::endl;
    std::cout << "  1-5         - Quality" << std::endl;
    std::cout << "  C/H         - Panels" << std::endl;
    std::cout << "  ESC         - Quit" << std::endl;
    std::cout << "==============================\n" << std::endl;

    // Create SDL renderer for UI
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Main loop
    bool running = true;
    bool need_render = true;
    SDL_Event event;

    auto last_frame_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float fps = 0.0f;
    double render_time = 0.0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_h) {
                    show_help = !show_help;
                } else if (event.key.keysym.sym == SDLK_c) {
                    show_controls = !show_controls;
                } else if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_5) {
                    int new_quality = event.key.keysym.sym - SDLK_1;
                    if (new_quality != current_quality && new_quality < NUM_QUALITY_LEVELS) {
                        current_quality = new_quality;
                        preset = quality_levels[current_quality];
                        std::cout << "Quality: " << preset.name << std::endl;
                        need_render = true;
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (!show_help && !show_controls) {
                        SDL_bool captured = SDL_GetRelativeMouseMode();
                        SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                    }
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
        float move_speed = 0.05f; // Slower speed

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
            auto render_start = std::chrono::high_resolution_clock::now();

            // OpenGL render
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(program);

            // Update uniforms
            glUniform2f(resolution_loc, (float)WIDTH, (float)HEIGHT);
            glUniform3f(camera_pos_loc, camera.position[0], camera.position[1], camera.position[2]);
            glUniform3f(camera_lookat_loc, camera.lookat[0], camera.lookat[1], camera.lookat[2]);
            glUniform3f(camera_vup_loc, camera.vup[0], camera.vup[1], camera.vup[2]);
            glUniform1f(camera_vfov_loc, camera.vfov);
            glUniform1i(max_depth_loc, preset.max_depth);

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            SDL_GL_SwapWindow(window);

            auto render_end = std::chrono::high_resolution_clock::now();
            render_time = std::chrono::duration<double>(render_end - render_start).count();

            // Calculate FPS
            frame_count++;
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_frame_time).count();
            if (elapsed >= 1.0) {
                fps = frame_count / elapsed;
                frame_count = 0;
                last_frame_time = now;
                std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                         << " | Quality: " << preset.name
                         << " | Pos: " << camera.position[0] << ", " << camera.position[1] << ", " << camera.position[2]
                         << "     " << std::flush;
            }

            need_render = false;
        }

        // Render UI overlays
        if (renderer) {
            SDL_RenderClear(renderer);

            int window_width, window_height;
            SDL_GetWindowSize(window, &window_width, &window_height);

            if (show_controls) {
                controls_panel.render(renderer, window_width, window_height,
                                    current_quality, preset, fps, render_time);
            }

            if (show_help) {
                help_overlay.render(renderer, window_width, window_height);
            }

            SDL_RenderPresent(renderer);
        }

        SDL_Delay(1);
    }

    std::cout << "\n=== Exiting ===" << std::endl;

    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }

    glDeleteProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Shut down successfully" << std::endl;
    return 0;
}