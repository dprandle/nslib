#version 450
#extension GL_EXT_debug_printf : enable

layout(binding = 0) uniform UBO {
    mat4 proj_view;
} ubo;

//push constants block
layout( push_constant ) uniform PC
{
    mat4 model;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in uvec4 in_color;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 frag_tex_coord;

void main() {
    // Because GLSL stores matrices in column major, we reverse our multiplication order
    gl_Position = vec4(in_pos, 1.0) * pc.model * ubo.proj_view;
    fragColor = vec4(in_color.r, in_color.g, in_color.b, in_color.a) / 255.0;
    frag_tex_coord = in_tex_coord;
}
