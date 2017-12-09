#version 330 core
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model[252];
uniform samplerBuffer u_tbo_tex;

void main()
{
    mat4 transform = mat4( texelFetch( u_tbo_tex, 0 + (gl_InstanceID * 4)),
    texelFetch( u_tbo_tex, 1 + (gl_InstanceID * 4)),
    texelFetch( u_tbo_tex, 2 + (gl_InstanceID * 4)),
    texelFetch( u_tbo_tex, 3 + (gl_InstanceID * 4)));
    TexCoords = uv;
    WorldPos = vec3(transform * vec4(vertex, 1.0));
    Normal = mat3(transform) * normal;

    gl_Position =  projection * view * vec4(WorldPos, 1.0);
}
