#include "ssa/level_script.h"
#include <string.h>

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)u16(p)|(uint32_t)u16(p+2)<<16; }
static int32_t signed32(uint32_t v) {
    return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);
}
static const uint8_t *bytes(const SsaLevelScriptData *d,uint32_t a,size_t n) {
    size_t o;
    if (!d || !d->rom || a<0x08000000u) return NULL;
    o=(size_t)(a-0x08000000u);
    if (o>d->size || n>d->size-o) return NULL;
    return d->rom+o;
}
int ssa_level_script_data_load(SsaLevelScriptData *out,const uint8_t *rom,size_t size) {
    SsaLevelScriptData d={0}; unsigned i,j;
    if (!out || !rom || size<0x45284c) return 0;
    d.rom=rom; d.size=size;
    for(i=0;i<16;++i) d.table[i]=u32(rom+0x45280c+4*i);
    for(i=0;i<16;++i) {
        uint32_t end=i<15?d.table[i+1]:0x0845280cu;
        if (d.table[i]>end || end>0x0845280cu || (end-d.table[i])%4 ||
            (end-d.table[i])/4>255 || !bytes(&d,d.table[i],end-d.table[i])) return 0;
        d.count[i]=(uint16_t)((end-d.table[i])/4);
        for(j=0;j<d.count[i];++j) {
            uint32_t pc=u32(bytes(&d,d.table[i]+4*j,4));
            if ((pc&3) || !bytes(&d,pc,20)) return 0;
        }
    }
    *out=d; return 1;
}
int ssa_level_script_reset(SsaLevelScript *s) {
    unsigned i;
    if (!s || s->updating) return 0;
    s->count=0;
    for(i=0;i<5;++i) { s->pc[i]=0; memset(s->state[i],0,12); }
    memset(s->alive,0,sizeof(s->alive)); memset(s->visited,0,sizeof(s->visited));
    return 1;
}
int ssa_level_script_queue(SsaLevelScript *s,const SsaLevelScriptData *d,unsigned level,unsigned id) {
    const uint8_t *p; uint32_t pc;
    if (!s || s->updating || !d || level>=16 || !id || id>d->count[level] ||
        s->count>=5 || !(p=bytes(d,d->table[level]+4*(id-1),4))) return 0;
    pc=u32(p); if ((pc&3) || !bytes(d,pc,20)) return 0;
    s->pc[s->count++]=pc; return 1;
}
int ssa_level_script_enter(SsaLevelScript *s,const SsaLevelScriptData *d,unsigned level,
                           unsigned sector,uint16_t flags) {
    if (!s || s->updating || sector>=256) return 0;
    if (s->visited[sector]) return 1;
    if ((flags&0x100) && !ssa_level_script_queue(s,d,level,flags&255)) return 0;
    s->visited[sector]=1; return 1;
}
int ssa_level_script_release(SsaLevelScript *s,uint16_t tag) {
    if (!s || s->updating) return 0;
    if (!(tag&255) || (tag&256)) return 1;
    if (tag>256) return 0;
    --s->alive[tag-1]; return 1;
}
int ssa_level_script_step(SsaLevelScript *s,const SsaLevelScriptData *d,unsigned ticks,
                          int32_t effect_z,const SsaLevelScriptOps *ops) {
    unsigned i;
    if (!s || s->updating || s->count>5 || !d || !d->rom || ticks>255 || !ops ||
        !ops->allocate || !ops->animation || !ops->insert || !ops->particle || !ops->mover) return -1;
    s->updating=1;
    for(i=0;i<s->count;++i) {
        uint32_t pc=s->pc[i],*st=s->state[i],op,arg; const uint8_t *p=bytes(d,pc,20);
        if (!p || (pc&3)) goto fail;
        op=u32(p); arg=u32(p+8);
        switch(op) {
        case 1: {
            const uint8_t *t,*def; unsigned x,y,type; SsaLevelScriptEntity *e;
            if (st[0]!=1) { st[0]=op;st[1]=u32(p+4);st[2]=arg;st[4]=u32(p+16); }
            st[4]-=ticks;
            if (signed32(st[4])>0) break;
            st[4]=u32(p+16);
            t=bytes(d,st[1],8); if (!t || (st[1]&1)) goto fail;
            x=u16(t);y=u16(t+2);
            if (!(x|y)) { st[0]=0;pc+=20;break; }
            type=u16(t+4); if (type>=64 || !(def=bytes(d,0x0844a3d4u+8*type,8))) goto fail;
            e=ops->allocate(ops->context);
            if (e) {
                e->x=signed32((uint32_t)x<<16); e->y=signed32((uint32_t)y<<16);
                e->type=u16(def);e->variant=(uint8_t)u16(def+2);
                e->facing=(uint8_t)((u16(t+6)>>8)+128);
                e->trigger=(uint16_t)arg;e->timer=60;e->flags|=0x80;
                if (ops->animation(ops->context,e,u16(def+4),u16(def+6))<0 ||
                    ops->insert(ops->context,e)<0) goto fail;
                e->z=signed32((uint32_t)e->floor<<16); effect_z=e->z;
                if (!(arg&256)) {
                    if(arg&255) ++s->alive[(arg&255)-1];
                    else ++s->alive_before;
                }
            }
            if (ops->particle(ops->context,5,signed32((uint32_t)x<<16),
                              signed32((uint32_t)y<<16),effect_z)<0) goto fail;
            st[1]+=8;break;
        }
        case 2:
            if (!arg || arg>256) goto fail;
            if (!s->alive[arg-1]) pc+=20;
            break;
        case 3: pc+=20;break;
        case 4: case 5: case 6: case 7:
            if (ops->mover(ops->context,op,arg)<0) goto fail;
            pc+=20;break;
        case 8:
            if (st[0]!=8) { st[0]=8;st[1]=u32(p+4);st[2]=arg*60u; }
            else { st[2]-=ticks; if(signed32(st[2])<=0) { st[0]=0;pc+=20; } }
            break;
        case 9: s->game_state=3;s->exit_toggle^=1;break;
        case 10:
            --s->count;s->pc[i]=s->pc[s->count];memcpy(st,s->state[s->count],20);
            pc=0;break;
        default: break;
        }
        if(pc) s->pc[i]=pc;
    }
    s->updating=0;return 0;
fail:
    s->updating=0;return -1;
}
