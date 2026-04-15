#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <thread>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
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

// Set cursor position (platform-specific)
void set_cursor_pos(int row, int col) {
#ifdef _WIN32
    COORD coord;
    coord.X = col;
    coord.Y = row;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
#else
    std::cout << "\033[" << row << ";" << col << "H";
    std::cout.flush();
#endif
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   ASCII RAY TRACER" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Initializing..." << std::endl;

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

    // Setup scene
    Scene scene;
    setup_cornell_box_scene(scene);

    // Setup camera - positioned to avoid central sphere during orbit
    Point3 lookfrom(0, 1, 8);
    Point3 lookat(0, 0.5, 0);
    Vec3 vup(0, 1, 0);
    float dist_to_focus = (lookfrom - lookat).length();
    float aperture = 0.0f;
    float vfov = 60.0f;
    float aspect_ratio = static_cast<float>(term_width) / term_height;

    Camera cam(lookfrom, lookat, vup, vfov, aspect_ratio, aperture, dist_to_focus);

    // Create renderer
    Renderer renderer(max_depth);
    renderer.enable_shadows = true;
    renderer.enable_reflections = true;

    // ASCII framebuffer
    std::vector<std::vector<char>> ascii_framebuffer(term_height, std::vector<char>(term_width));

    std::cout << "\nStarting rendering..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;

    // Small delay to read the message
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Render loop (limited frames for testing)
    for (int frame = 0; frame < 100; frame++) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        // Clear screen once per frame
        clear_screen();

        // Render ASCII art
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
        std::cout << "Frame " << (frame + 1) << " | "
                  << frame_time << "s | "
                  << mrays << " MRays/s | "
                  << samples << " samples | "
                  << term_width << "x" << term_height;

        // Simple animation - move camera slightly
        float angle = frame * 0.02f;
        lookfrom.x = std::sin(angle) * 8.0f;  // Increased radius to avoid central sphere
        lookfrom.z = std::cos(angle) * 8.0f;  // Increased radius to avoid central sphere
        cam = Camera(lookfrom, lookat, vup, vfov, aspect_ratio, aperture, dist_to_focus);

        // Small delay to control frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}