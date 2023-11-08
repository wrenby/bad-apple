#version 410 core

uniform vec3 lightDir; // must be normalized
layout (location = 0) in vec3 norm;

layout (location = 0) out float intensity;

void main() {
    // intensity = min(dot(normalize(norm), lightDir), 0.0f);
    intensity = 1.0f;
}