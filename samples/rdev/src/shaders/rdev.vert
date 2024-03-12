#version 450

layout(binding = 0) uniform UBO {
    mat4 proj_view;
} ubo;

//push constants block
layout( push_constant ) uniform PC
{
    mat4 model;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 frag_tex_coord;

void main() {
    // Because GLSL stores matrices in column major, we reverse our multiplication order
    gl_Position = ubo.proj_view  * pc.model * vec4(in_pos, 1.0);
    fragColor = in_color;
    frag_tex_coord = in_tex_coord;
}
