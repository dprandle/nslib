#version 450

layout(binding = 0) uniform uniform_buffer_object {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 fragColor;

void main() {
    // Because GLSL stores matrices in column major, we reverse our multiplication order
    gl_Position = vec4(in_pos, 0.0, 1.0) * ubo.model * ubo.view * ubo.proj;
    fragColor = in_color;
}
