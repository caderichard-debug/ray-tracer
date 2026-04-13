#version 410 core

// Vertex shader for rendering a screen-filling quad
// Used to display the compute shader ray tracing output

layout(location = 0) in vec2 a_position;

// Output texture coordinates to fragment shader
layout(location = 0) out vec2 v_tex_coord;

void main() {
    // Full-screen quad: (-1, -1) to (1, 1)
    gl_Position = vec4(a_position, 0.0, 1.0);

    // Texture coordinates: (0, 0) to (1, 1)
    v_tex_coord = a_position * 0.5 + 0.5;
    // Flip Y axis because OpenGL textures origin is bottom-left
    v_tex_coord.y = 1.0 - v_tex_coord.y;
}
