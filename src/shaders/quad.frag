#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform sampler2D screen_texture;

void main() {
    // Sample the rendered texture
    vec3 color = texture(screen_texture, uv).rgb;

    // Simple gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}