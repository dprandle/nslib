#version 450
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 0) out vec4 outColor;

void main() {
    //outColor = texture(tex_sampler, frag_tex_coord);
    outColor = fragColor;
}
