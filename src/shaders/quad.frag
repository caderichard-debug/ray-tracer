#version 410 core

// Fragment shader for displaying the ray tracing output
// Applies tone mapping and outputs to the screen

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 frag_color;

// The ray tracing output texture
layout(binding = 0) uniform sampler2D u_texture;

void main() {
    // Sample the ray tracing output
    vec3 color = texture(u_texture, v_tex_coord).rgb;

    // Simple tone mapping (Reinhard)
    color = color / (color + vec3(1.0));

    // Gamma correction (already applied in compute shader, but ensure sRGB output)
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}
