#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <thread>
#include <cstdlib>
#include <atomic>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

#include "math/vec3.h"
#include "math/ray.h"
#include "primitives/sphere.h"
#include "primitives/primitive.h"
#include "material/material.h"
#include "camera/camera.h"
#include "scene/scene.h"
#include "scene/light.h"
#include "scene/cornell_box.h"
#include "renderer/renderer.h"

// ASCII character set from darkest to lightest
const char ASCII_CHARS[] = "  .:-=+*#%@";
const int ASCII_CHAR_COUNT = 10;

// Terminal dimensions
int term_width = 80;
int term_height = 24;

// Camera controller state
struct CameraController {
    Point3 position;
    Point3 lookat;
    Vec3 vup;
    float vfov;
    float aspect_ratio;
    float aperture;
    float dist_to_focus;

    float yaw;   // Horizontal rotation
    float pitch; // Vertical rotation

    CameraController()
        : position(0, 1, 4), lookat(0, 0.5, 0), vup(0, 1, 0),
          vfov(60.0f), aspect_ratio(16.0f / 9.0f), aperture(0.0f),
          dist_to_focus(3.0f), yaw(-90.0f), pitch(0.0f) {
        update_from_angles();
    }

    void update_from_angles() {
        // Convert spherical coordinates to cartesian
        float yaw_rad = yaw * M_PI / 180.0f;
        float pitch_rad = pitch * M_PI / 180.0f;

        Vec3 direction;
        direction.x = cos(yaw_rad) * cos(pitch_rad);
        direction.y = sin(pitch_rad);
        direction.z = sin(yaw_rad) * cos(pitch_rad);

        lookat = position + direction;
    }

    void move_forward(float delta) {
        Vec3 forward = (lookat - position).normalized();
        position = position + forward * delta;
        update_from_angles();
    }

    void move_right(float delta) {
        Vec3 forward = (lookat - position).normalized();
        Vec3 right = cross(forward, vup).normalized();
        position = position + right * delta;
        update_from_angles();
    }

    void move_up(float delta) {
        position.y += delta;
        update_from_angles();
    }

    void rotate(float delta_yaw, float delta_pitch) {
        yaw += delta_yaw;
        pitch += delta_pitch;

        // Clamp pitch to avoid gimbal lock
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        update_from_angles();
    }

    Camera get_camera() const {
        return Camera(position, lookat, vup, vfov, aspect_ratio, aperture, dist_to_focus);
    }
};

// Get terminal size
void get_terminal_size() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    term_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    term_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    term_width = w.ws_col;
    term_height = w.ws_row;
#endif

    // Reserve space for UI and ensure even dimensions
    term_height -= 3; // Space for status line
    if (term_height < 10) term_height = 10;
    if (term_width < 40) term_width = 40;
}

// Map color to grayscale (0-1)
float color_to_grayscale(const Color& color) {
    // Standard luminance formula: 0.299*R + 0.587*G + 0.114*B
    return 0.299f * color.x + 0.587f * color.y + 0.114f * color.z;
}

// Map grayscale (0-1) to ASCII character
char grayscale_to_ascii(float gray) {
    // Clamp grayscale to [0, 1]
    if (gray < 0.0f) gray = 0.0f;
    if (gray > 1.0f) gray = 1.0f;

    // Map to ASCII character index
    int index = static_cast<int>(gray * (ASCII_CHAR_COUNT - 1));
    return ASCII_CHARS[index];
}

// Clear screen (platform-specific)
void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Set terminal to non-blocking mode (Unix/Linux/macOS)
#ifndef _WIN32
void set_nonblocking_mode() {
    termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 0;
    ttystate.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void restore_terminal_mode() {
    termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}
#endif

// Check for keypress (non-blocking)
bool key_pressed(int& key) {
#ifdef _WIN32
    if (_kbhit()) {
        key = _getch();
        // Handle arrow keys (Windows sends 224 first)
        if (key == 224) {
            key = _getch();
        }
        return true;
    }
    return false;
#else
    char c;
    if (read(STDIN_FILENO, &c, 1) > 0) {
        key = c;
        return true;
    }
    return false;
#endif
}

// Show help overlay
void show_help() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "     ASCII RAY TRACER CONTROLS\n";
    std::cout << "========================================\n";
    std::cout << "MOVEMENT:\n";
    std::cout << "  W/S - Move forward/backward\n";
    std::cout << "  A/D - Move left/right\n";
    std::cout << "  Q/E - Move up/down\n";
    std::cout << "LOOK:\n";
    std::cout << "  Arrow Keys - Look around\n";
    std::cout << "OTHER:\n";
    std::cout << "  H - Toggle this help\n";
    std::cout << "  ESC - Quit\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    std::cout << "Press any key to start...\n";
    std::cout << std::flush;

    // Wait for keypress
    int key;
    while (!key_pressed(key)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   ASCII RAY TRACER - INTERACTIVE" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Initializing..." << std::endl;

    // Setup non-blocking input
#ifndef _WIN32
    set_nonblocking_mode();
#endif

    // Get terminal size
    get_terminal_size();
    std::cout << "Terminal size: " << term_width << "x" << term_height << std::endl;

    // Ask user for quality preset
    std::cout << "\nQuality presets:" << std::endl;
    std::cout << "1. Low (1 sample, fast)" << std::endl;
    std::cout << "2. Medium (4 samples, balanced)" << std::endl;
    std::cout << "3. High (16 samples, slow)" << std::endl;
    std::cout << "\nChoose quality (1-3): ";

    int quality_choice = 2;
    std::cin >> quality_choice;

    int samples, max_depth;
    switch (quality_choice) {
        case 1: samples = 1; max_depth = 3; break;
        case 3: samples = 16; max_depth = 5; break;
        default: samples = 4; max_depth = 4; break;
    }

    std::cout << "\nRendering with " << samples << " samples, max depth " << max_depth << std::endl;
    std::cout << "Resolution: " << term_width << "x" << term_height << std::endl;

    // Show help
    show_help();

    // Setup scene
    Scene scene;
    setup_cornell_box_scene(scene);

    // Create camera controller
    CameraController cam_controller;

    // Create renderer
    Renderer renderer(max_depth);
    renderer.enable_shadows = true;
    renderer.enable_reflections = true;

    // ASCII framebuffer
    std::vector<std::vector<char>> ascii_framebuffer(term_height, std::vector<char>(term_width));

    std::cout << "Starting interactive rendering..." << std::endl;
    std::cout << "Press H for help, ESC to quit" << std::endl;
    std::cout << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Render loop
    std::atomic<bool> running(true);
    std::atomic<bool> show_help_overlay(false);
    int frame_count = 0;
    bool need_render = true;

    while (running) {
        // Handle keyboard input
        int key = 0;
        while (key_pressed(key)) {
            switch (key) {
                case 27: // ESC
                    running = false;
                    break;
                case 'h':
                case 'H':
                    show_help_overlay = !show_help_overlay;
                    break;
                case 'w':
                case 'W':
                    cam_controller.move_forward(0.2f);
                    need_render = true;
                    break;
                case 's':
                case 'S':
                    cam_controller.move_forward(-0.2f);
                    need_render = true;
                    break;
                case 'a':
                case 'A':
                    cam_controller.move_right(-0.2f);
                    need_render = true;
                    break;
                case 'd':
                case 'D':
                    cam_controller.move_right(0.2f);
                    need_render = true;
                    break;
                case 'q':
                case 'Q':
                    cam_controller.move_up(0.2f);
                    need_render = true;
                    break;
                case 'e':
                case 'E':
                    cam_controller.move_up(-0.2f);
                    need_render = true;
                    break;
#ifdef _WIN32
                // Windows arrow keys
                case 72: // Up
                    cam_controller.rotate(0.0f, 2.0f);
                    need_render = true;
                    break;
                case 80: // Down
                    cam_controller.rotate(0.0f, -2.0f);
                    need_render = true;
                    break;
                case 75: // Left
                    cam_controller.rotate(-2.0f, 0.0f);
                    need_render = true;
                    break;
                case 77: // Right
                    cam_controller.rotate(2.0f, 0.0f);
                    need_render = true;
                    break;
#else
                // Unix/Linux/macOS arrow keys (escape sequences)
                case 91: // Part of arrow sequence
                    // Read the next character to determine arrow direction
                    int arrow_key;
                    if (key_pressed(arrow_key)) {
                        switch (arrow_key) {
                            case 65: // Up
                                cam_controller.rotate(0.0f, 2.0f);
                                need_render = true;
                                break;
                            case 66: // Down
                                cam_controller.rotate(0.0f, -2.0f);
                                need_render = true;
                                break;
                            case 68: // Left
                                cam_controller.rotate(-2.0f, 0.0f);
                                need_render = true;
                                break;
                            case 67: // Right
                                cam_controller.rotate(2.0f, 0.0f);
                                need_render = true;
                                break;
                        }
                    }
                    break;
#endif
            }
        }

        if (!running) break;

        // Render frame if needed
        if (need_render || show_help_overlay) {
            auto frame_start = std::chrono::high_resolution_clock::now();

            clear_screen();

            if (show_help_overlay) {
                // Show help overlay instead of scene
                std::cout << "\n";
                std::cout << "========================================\n";
                std::cout << "     ASCII RAY TRACER - CONTROLS\n";
                std::cout << "========================================\n";
                std::cout << "\n";
                std::cout << "MOVEMENT:\n";
                std::cout << "  W/S - Move forward/backward\n";
                std::cout << "  A/D - Move left/right\n";
                std::cout << "  Q/E - Move up/down\n";
                std::cout << "\n";
                std::cout << "LOOK:\n";
                std::cout << "  Arrow Keys - Look around\n";
                std::cout << "\n";
                std::cout << "OTHER:\n";
                std::cout << "  H - Toggle this help\n";
                std::cout << "  ESC - Quit\n";
                std::cout << "\n";
                std::cout << "========================================\n";
                std::cout << "\n";
                std::cout << "Current Position: " << cam_controller.position << std::endl;
                std::cout << "Press any key to continue...\n";
                std::cout << std::flush;

                // Wait for keypress to continue
                int wait_key;
                while (!key_pressed(wait_key) && running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                show_help_overlay = false;
                need_render = true;
                continue;
            }

            // Get camera from controller
            Camera cam = cam_controller.get_camera();

            // Render ASCII art
            #pragma omp parallel for schedule(dynamic, 4)
            for (int j = term_height - 1; j >= 0; --j) {
                for (int i = 0; i < term_width; ++i) {
                    Color pixel_color(0, 0, 0);

                    // Sample the pixel
                    for (int s = 0; s < samples; ++s) {
                        auto random_float = []() {
                            return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        };

                        float u = (i + random_float()) / (term_width - 1);
                        float v = (j + random_float()) / (term_height - 1);

                        Ray r = cam.get_ray(u, v);
                        Color sample_color = renderer.ray_color(r, scene, max_depth);
                        pixel_color = pixel_color + sample_color;
                    }

                    // Average samples
                    pixel_color = pixel_color / samples;

                    // Apply gamma correction
                    pixel_color.x = std::sqrt(pixel_color.x);
                    pixel_color.y = std::sqrt(pixel_color.y);
                    pixel_color.z = std::sqrt(pixel_color.z);

                    // Convert to ASCII
                    float gray = color_to_grayscale(pixel_color);
                    ascii_framebuffer[term_height - 1 - j][i] = grayscale_to_ascii(gray);
                }
            }

            // Output ASCII art
            for (int j = 0; j < term_height; ++j) {
                for (int i = 0; i < term_width; ++i) {
                    std::cout << ascii_framebuffer[j][i];
                }
                std::cout << std::endl;
            }

            // Calculate performance
            auto frame_end = std::chrono::high_resolution_clock::now();
            double frame_time = std::chrono::duration<double>(frame_end - frame_start).count();

            // Status line
            int pixels = term_width * term_height;
            int rays = pixels * samples;
            double mrays = rays / 1000000.0 / frame_time;

            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Frame " << (++frame_count) << " | "
                      << frame_time << "s | "
                      << mrays << " MRays/s | "
                      << samples << " samples | "
                      << "Pos: " << cam_controller.position
                      << " | H=Help";

            need_render = false;
        }

        // Small delay to prevent 100% CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup
#ifndef _WIN32
    restore_terminal_mode();
#endif

    clear_screen();
    std::cout << "Thanks for using the ASCII Ray Tracer!" << std::endl;
    std::cout << "Total frames rendered: " << frame_count << std::endl;

    return 0;
}