#version 430 core

layout (location = 1) uniform vec3 lightDir; // must be normalized

layout (location = 0) in vec3 norm;

layout (location = 0) out float intensity;

void main() {
    intensity = clamp((dot(normalize(norm), -lightDir)), 0.1f, 1.0f);
}