#version 450
layout(location=0) in vec2 texcoord;
layout(location=1) in float distanceZ;
layout(location=2) flat in uint mat;
layout(std430,binding=0) readonly buffer Texels {uint texels[];};
struct Material {uvec4 base;uvec4 mip1;uvec4 mip2;};
layout(std430,binding=1) readonly buffer Materials {Material materials[];};
layout(location=0) out vec4 color;
layout(location=1) out vec4 sampleData;
void main() {
    uvec4 m=materials[mat].base; // offset,width,height,packed flags/palette index
    uint index=m.w&255u;
    if(m.y!=0u) {
        if((m.w&8192u)!=0u && (any(lessThan(texcoord,vec2(0))) || any(greaterThanEqual(texcoord,vec2(m.yz)))))discard;
        uvec4 sampleMip=m;vec2 uv=texcoord;
        if(materials[mat].mip1.y!=0u){
            vec2 dx=dFdx(texcoord),dy=dFdy(texcoord);
            float footprint2=max(dot(dx,dx),dot(dy,dy));
            if(footprint2>=16.0)sampleMip=materials[mat].mip2;
            else if(footprint2>=4.0)sampleMip=materials[mat].mip1;
            uv*=vec2(sampleMip.yz)/vec2(m.yz);
        }
        ivec2 p=ivec2(mod(floor(uv),vec2(sampleMip.yz)));
        index=texels[sampleMip.x+uint(p.x)*sampleMip.z+uint(p.y)];
        if((m.w&8192u)!=0u && index==0u)discard;
    }
    uint rgb=texels[index];
    color=vec4(float((rgb>>16)&255u),float((rgb>>8)&255u),float(rgb&255u),255.0)/255.0;
    if((m.w&1024u)!=0u)color.a=0.0; // Sky writes depth, keeps the flat background.
    sampleData=vec4(texcoord,distanceZ,float((m.w&~255u)|index));
}
