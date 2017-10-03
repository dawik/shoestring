#version 150

in vec3 vertex;
in vec3 normal;
in vec2 uv;

uniform mat4 model;
uniform mat4 camera;
uniform mat4 projection;
uniform vec4 color;

out vec3 vertexFrag;
out vec3 normalFrag;
out vec2 uvFrag;

void main() {
        gl_Position = projection * camera * model * vec4(vertex, 1.0);
        uvFrag = uv;
        normalFrag = normal;
        vertexFrag = vertex;
}
