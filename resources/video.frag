#version 430 core

layout (location = 0) uniform sampler2D frame;

layout (location = 0) in vec2 fTexCoord;

layout (location = 0) out float intensity;

void main() {
    vec3 rgb = vec3(texture(frame, fTexCoord));
    vec3 coefLuma = vec3(0.299, 0.587, 0.114); // ITU BT.601: perceived luminance calculation
    intensity = dot(coefLuma, rgb);
}