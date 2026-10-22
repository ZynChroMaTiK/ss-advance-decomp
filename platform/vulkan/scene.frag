#version 450
layout(push_constant) uniform View {vec2 scale;uint anisotropic;} view;
layout(location=0) in vec2 texcoord;
layout(location=1) in float distanceZ;
layout(location=2) flat in uint mat;
layout(std430,binding=0) readonly buffer Texels {uint texels[];};
struct Material {uvec4 base;uvec4 mip1;uvec4 mip2;};
layout(std430,binding=1) readonly buffer Materials {Material materials[];};
layout(location=0) out vec4 color;
layout(location=1) out vec4 sampleData;
vec3 paletteColor(uint index) {
    uint rgb=texels[index];return vec3(float((rgb>>16)&255u),float((rgb>>8)&255u),float(rgb&255u))/255.0;
}
// Keep magnified texel edges exact; anisotropic taps only integrate along
// the minified footprint, never bilinearly blend across the sharp axis.
vec3 filteredTexel(uvec4 m,vec2 uv) {
    ivec2 p=ivec2(mod(floor(uv),vec2(m.yz)));
    return paletteColor(texels[m.x+uint(p.x)*m.z+uint(p.y)]);
}
vec3 anisotropicColor(uvec4 m,vec2 dx,vec2 dy) {
    // Principal axes of the projected texel footprint, bounded to8 samples.
    float a=dx.x*dx.x+dy.x*dy.x,b=dx.x*dx.y+dy.x*dy.y,c=dx.y*dx.y+dy.y*dy.y;
    float disc=sqrt(max(0.0,(a-c)*(a-c)+4.0*b*b));
    float major=sqrt(max(1.0,0.5*(a+c+disc))),minor=sqrt(max(1.0,0.5*(a+c-disc)));
    vec2 axis=abs(b)>0.00001?normalize(vec2(b,0.5*(a+c+disc)-a)):(a>=c?vec2(1,0):vec2(0,1));
    int taps=int(clamp(ceil(major/minor),1.0,8.0));
    // When anisotropy exceeds8x, increase the minor footprint to bound aliasing.
    minor=max(minor,major/8.0);uvec4 mip=m;
    if(materials[mat].mip1.y!=0u){
        if(minor>=4.0)mip=materials[mat].mip2;
        else if(minor>=2.0)mip=materials[mat].mip1;}
    vec3 sum=vec3(0);vec2 ratio=vec2(mip.yz)/vec2(m.yz);
    for(int i=0;i<taps;++i){vec2 uv=texcoord+axis*major*((float(i)+0.5)/float(taps)-0.5);
        sum+=filteredTexel(mip,uv*ratio);}
    return sum/float(taps);
}
void main() {
    uvec4 m=materials[mat].base; // offset,width,height,packed flags/palette index
    uint index=m.w&255u;
    vec2 dx=dFdx(texcoord),dy=dFdy(texcoord);
    if(m.y!=0u) {
        if((m.w&8192u)!=0u && (any(lessThan(texcoord,vec2(0))) || any(greaterThanEqual(texcoord,vec2(m.yz)))))discard;
        uvec4 sampleMip=m;vec2 uv=texcoord;
        if(materials[mat].mip1.y!=0u){
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
    if(view.anisotropic!=0u && m.y!=0u && (m.w&(2048u|8192u))==0u)color.rgb=anisotropicColor(m,dx,dy);
    if((m.w&1024u)!=0u)color.a=0.0; // Sky writes depth, keeps the flat background.
    sampleData=vec4(texcoord,distanceZ,float((m.w&~255u)|index));
}
