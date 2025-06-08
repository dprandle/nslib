#version 450
#extension GL_EXT_debug_printf : enable

layout(set = 0, binding = 0) uniform Frame_UBO {
    ivec4 frame_count;
} frame_ubo;

layout(set = 1, binding = 0) uniform Pipeline_UBO {
    mat4 proj_view;
} pl_ubo;

layout(set = 2, binding = 0) uniform Material_UBO {
    vec4 color;
} mat_ubo;

layout(set = 3, binding = 0) uniform Object_UBO {
    mat4 transform;
} obj_ubo;

//push constants block
layout( push_constant ) uniform PC
{
    uvec4 test;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in uvec4 in_color;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 frag_tex_coord;

void main() {
    // Because GLSL stores matrices in column major, we reverse our multiplication order
    gl_Position = vec4(in_pos, 1.0) * obj_ubo.transform * pl_ubo.proj_view;
    fragColor = mat_ubo.color;
    frag_tex_coord = in_tex_coord;
}
