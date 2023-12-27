#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 frag_tex_coord;

layout(location = 0) out vec4 outColor;
layout(binding = 1) uniform sampler2D tex_sampler;

void main() {
    outColor = texture(tex_sampler, frag_tex_coord);
}
