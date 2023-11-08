#version 410 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNorm;

layout (location = 0) out vec3 fNorm;

void main() {
    fNorm = normalize(vNorm);
    gl_Position = vec4(vPos, 1.0);
}