#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in uint material;
layout(push_constant) uniform View {vec2 scale;uint anisotropic;} view;
layout(location=0) out vec2 texcoord;
layout(location=1) out float distanceZ;
layout(location=2) flat out uint mat;
void main() {
    // Reversed depth retains precision in large maps. CPU clips at0.05 units.
    gl_Position=vec4(position.xy*view.scale,0.05,position.z);
    texcoord=uv; distanceZ=position.z; mat=material&0x7fffffffu;
}
