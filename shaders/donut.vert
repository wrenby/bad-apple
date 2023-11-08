#version 430 core

layout (location = 0) uniform mat4 mvp;

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNorm;

layout (location = 0) out vec3 fNorm;

void main() {
    fNorm = normalize(vec3(mvp * vec4(vNorm, 0.0)));
    gl_Position = mvp * vec4(vPos, 1.0);
}