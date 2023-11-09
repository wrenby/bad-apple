#version 430 core

layout (location = 0) uniform mat4 mvp;

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec2 vTexCoord;

layout (location = 0) out vec4 fPos;
layout (location = 1) out vec2 fTexCoord;

void main() {
    fPos = mvp * vec4(vPos, 1.0);
    fTexCoord = vTexCoord;
}