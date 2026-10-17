#include "ssa/hud_sprites.h"
#include <limits.h>
#include <string.h>

static int make(SsaSpritePool *p,int32_t x,int32_t y,unsigned type,int subtype,
                unsigned animation,unsigned scale,int hidden,unsigned marked)
{
    SsaWeaponObject *o;
    if(ssa_sprite_pool_allocate(p,&o)!=1)return -1;
    o->x=x;o->y=y;o->type=(uint16_t)type;if(subtype>=0)o->subtype=(uint16_t)subtype;
    o->layer=15;if(scale)o->scale_x=o->scale_y=(uint16_t)scale;
    if(ssa_weapon_animation_set(o,(uint16_t)animation,5)!=1)return -1;
    if(hidden)o->flags&=~UINT32_C(1);
    if(marked)o->flags|=UINT32_C(0x04000000);
    return 1;
}
int ssa_hud_sprites_create(SsaSpritePool *p,unsigned players,unsigned motion_player,unsigned level)
{
    unsigned i,n;
    if(!p || p->loaded!=1 || p->updating || !p->dispatch || p->dispatch->updating ||
       players<1 || players>4 || motion_player>=players || level>15)return -1;
    n=players==1?74:59+2*players;if(p->free_count<n)return -1;
#define MAKE(x,y,t,s,a,z,h,m) do{if(make(p,x,y,t,s,a,z,h,m)<0)return -1;}while(0)
    MAKE(0x2a00,0x9400,15,2,0x55,0,0,0);
    MAKE(0x2a00,0x9400,4,2,0x54,0,0,0);
    MAKE(0x1400,0x9700,4,2,0x56,0,0,0);
    if(players==1){
        MAKE(0x1800,0x1800,4,-1,0x5b,0,0,0);
        MAKE(0x2000,0x1000,16,-1,0x74,0,0,0);
    }else for(i=0;i<players;++i){
        MAKE(0x1800+(int32_t)i*0x2800,0x1000,4,-1,0xdb+i,0,0,i!=motion_player);
        MAKE(0x2400+(int32_t)i*0x2800,0x800,17,(int)i,0x74,0,0,i!=motion_player);
    }
    MAKE(0x3200,0x8800,15,1,0x55,0,0,0);
    MAKE(0x3200,0x8800,15,1,0x54,0,0,0);
    MAKE(0x1c00,0x8b00,15,1,0x57,0,0,0);
    MAKE(0x3a00,0x7c00,15,0,0x55,0,0,0);
    MAKE(0x3a00,0x7c00,15,0,0x54,0,0,0);
    MAKE(0x2400,0x7f00,15,0,level==5?0x59:0x5a,0,0,0);
    MAKE(0x7a00,0x5000,18,-1,0xd9,0,0,0);
    MAKE(0xc800,0x1000,7,-1,0x42,0,0,0);
    MAKE(0xc800,0xc800,8,-1,0xdf,0,0,0);
    for(i=0;i<3;++i)MAKE(0xc800,0xc800,6,(int)i,0x5c,0,1,0);
    for(i=0;i<2;++i)MAKE(0x7800,0x5000,14,(int)i,0x4c,1,1,0);
    if(players==1){
        for(i=0;i<8;++i)MAKE(0,0x800,9,(int)i,0x5c,0,1,0);
        for(i=0;i<5;++i)MAKE(0,0x1800,12,(int)i,0x5c,0x800,1,0);
    }
    for(i=0;i<42;++i)MAKE(0,0x3000,13,(int)i,0x5c,0x800,1,0);
#undef MAKE
    return (int)n;
}
static int32_t bits(uint32_t u){return u<=INT32_MAX?(int32_t)u:-1-(int32_t)~u;}
static int32_t asr(int32_t x,unsigned n){return bits(((uint32_t)x>>n)|(x<0?UINT32_MAX<<(32-n):0));}
static void visibility(SsaWeaponObject *o,uint16_t flags)
{
    if(flags&8)o->flags&=~UINT32_C(1);else o->flags|=1;
}
int ssa_hud_glyphs_load(SsaHudSpriteState *s,const void *data,size_t size)
{
    const uint8_t *rom=data;if(!s || !rom || size<0x44bc8c)return -1;
    memcpy(s->glyph_widths,rom+0x44bc0c,128);s->glyphs_loaded=1;return 1;
}
static unsigned decimal(uint32_t value,uint8_t out[12])
{
    uint8_t reverse[12];unsigned n=0,i;
    do{reverse[n++]=(uint8_t)(value%10+48);value/=10;}while(value);
    for(i=0;i<n;++i)out[i]=reverse[n-1-i];
    return n;
}
static int layout(SsaHudSpriteState *s,uint16_t *x,uint16_t *image,unsigned capacity,
                    const uint8_t *text,unsigned n,unsigned kind)
{
    unsigned i,start,advance=0,previous=0;int offset;
    if(s->glyphs_loaded!=1 || n>capacity)return -1;
    for(i=0;i<n;++i)if(text[i]<33 || text[i]>=161)return -1;
    memset(x,0,capacity*2);memset(image,0,capacity*2);start=capacity-n;
    for(i=0;i<n;++i){unsigned slot=start+i;
        image[slot]=(uint16_t)(text[i]+59);advance+=previous>>(kind==2?2:1);x[slot]=(uint16_t)advance;
        previous=s->glyph_widths[text[i]-33];advance+=1+(previous>>1);
        if(kind==1 && (slot==1 || slot==4))advance+=4;
    }
    offset=kind?120-(int)(advance>>1):208-(int)advance;
    for(i=start;i<capacity;++i)x[i]=(uint16_t)((int)x[i]+offset);
    return 1;
}
static int number(SsaWeaponObject *o,SsaHudSpriteState *s,unsigned kind)
{
    unsigned capacity=kind==0?3:kind==1?8:5,index,n;uint16_t *x,*image;uint8_t text[12];uint32_t value;
    if(o->subtype>=capacity)return -1;
    if(kind==0 && !(s->local_flags&8) && (s->current_weapon<1 || s->current_weapon>10))return -1;
    visibility(o,s->local_flags);if(s->local_flags&8)return 1;
    if(kind==1)o->flags&=~UINT32_C(0x04000000);
    if(kind==0 && s->current_weapon<=2){s->ammo[s->current_weapon-1]=999;o->y=0xc800;return 1;}
    if(kind==0)o->y=0x8c00;
    index=capacity-1-o->subtype;
    x=kind==0?s->ammo_x:kind==1?s->score_x:s->time_x;
    image=kind==0?s->ammo_image:kind==1?s->score_image:s->time_image;
    if(index==0){
        if(kind==2){uint32_t seconds=s->time/60,minutes=seconds/60;seconds%=60;
            if(minutes>99)return -1;
            n=decimal(minutes,text);text[n++]=58;
            text[n++]=(uint8_t)(48+seconds/10);text[n++]=(uint8_t)(48+seconds%10);
        }else{value=kind==0?(uint32_t)(int32_t)s->ammo[s->current_weapon-1]:s->score;n=decimal(value,text);}
        if(layout(s,x,image,capacity,text,n,kind)<0)return -1;
    }
    if(!image[index]){o->flags&=~UINT32_C(1);return 1;}
    o->x=(int32_t)(int16_t)x[index]*256;
    (void)ssa_weapon_animation_set(o,image[index],5);return 1;
}
int ssa_hud_sprite_step(SsaWeaponObject *o,SsaHudSpriteState *s)
{
    static const uint8_t icons[10]={0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b};
    static const uint8_t ammo[10]={0xdf,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8};
    int32_t a,b;unsigned animation;
    if(!o || !s)return -1;
    switch(o->type){
    case 4:visibility(o,s->local_flags);return 1; /* F0B4 is an empty return. */
    case 5:
        visibility(o,s->local_flags);if(s->local_flags&8)return 1;
        if(o->subtype>2)return 1;
        animation=(uint32_t)(int32_t)s->health;
        if(o->subtype==0)animation/=100;else if(o->subtype==1)animation/=10;
        (void)ssa_weapon_animation_set(o,(uint16_t)(animation%10+0x24),5);return 1;
    case 6:return number(o,s,0);
    case 9:return number(o,s,1);
    case 12:return number(o,s,2);
    case 13:
        if(!(s->local_flags&8) && s->notice_timer)return 0;
        s->game_flags&=(uint16_t)~0x200u;o->flags&=~UINT32_C(1);return 1;
    case 14:
        if((s->game_flags&0x41)!=0x41){o->flags&=~UINT32_C(1);return 1;}
        if(!o->subtype){int32_t w=(int32_t)s->rectangle[2]*1024,h=(int32_t)s->rectangle[3]*1024;
            o->flags|=1;
            if(w>0x1ffff){s->rectangle_tail[0]=(int32_t)s->rectangle[0]*65536+0x7f0000;
                s->rectangle_tail[1]=(int32_t)s->rectangle[1]*65536;s->rectangle_tail[2]=w-0x20000;s->rectangle_tail[3]=h;w=0x1ffff;
            }else s->rectangle_tail[0]=-1;
            o->x=(int32_t)s->rectangle[0]*256;o->y=(int32_t)s->rectangle[1]*256;
            o->scale_x=(uint16_t)(w/16);o->scale_y=(uint16_t)(h/16);
        }else if(s->rectangle_tail[0]<0)o->flags&=~UINT32_C(1);
        else{o->flags|=1;o->x=asr(s->rectangle_tail[0],8);o->y=asr(s->rectangle_tail[1],8);
            o->scale_x=(uint16_t)asr(s->rectangle_tail[2],4);o->scale_y=(uint16_t)asr(s->rectangle_tail[3],4);}
        return 1;
    case 15:
        if(!(s->local_flags&8) && o->subtype>2)return -1;
        visibility(o,s->local_flags);if(s->local_flags&8)return 1;
        if(o->subtype==0){o->y=0xc800;a=0;
            if(s->boss_present && s->boss_health){a=s->boss_health/30;
                o->x=s->armor?0x3a00:0x3200;o->y=s->armor?0x7c00:0x8800;}
        }else if(o->subtype==1){a=s->armor;if(!a)o->y=0xc800;else{o->x=0x3200;o->y=0x8800;}}
        else{a=s->health;o->y=a?0x9400:0xc800;}
        if(o->animation==0x55){
            if(a>99)o->scale_x=4096;
            else{b=asr(bits(UINT32_C(0x295fad)*(uint32_t)a),16);o->scale_x=(uint16_t)b;if(!b)o->scale_x=16;}
            o->scale_x=(uint16_t)(o->scale_x*2u);
        }else if(o->animation!=0x54){o->x=bits((uint32_t)o->x-0x1600u);o->y=bits((uint32_t)o->y+0x300u);}
        return 1;
    case 7:case 8:
        visibility(o,s->local_flags);if(s->local_flags&8)return 1;
        animation=o->type==7?0x4b:0xe8;
        if(s->current_weapon>=1 && s->current_weapon<=10)
            animation=o->type==7?icons[s->current_weapon-1]:ammo[s->current_weapon-1];
        if(o->type==8)o->y=s->current_weapon==1?0xc800:0x8c00;
        (void)ssa_weapon_animation_set(o,(uint16_t)animation,5);return 1;
    case 16:
        visibility(o,s->local_flags);if(!(s->local_flags&8))
            (void)ssa_weapon_animation_set(o,(uint16_t)(s->game_value+0x6b),5);
        return 1;
    case 17:
        if(s->skip_player_value){s->skip_player_value=0;return 1;}
        if(!(s->local_flags&8) && o->subtype>=4)return -1;
        visibility(o,s->local_flags);if(!(s->local_flags&8))
            (void)ssa_weapon_animation_set(o,(uint16_t)(s->player_values[o->subtype]+0x6b),5);
        return 1;
    case 18:
        if((s->local_flags&0x18) || s->weapon_action==1 || s->weapon_action==8){o->flags&=~UINT32_C(1);return 1;}
        o->flags|=1;
        a=bits((uint32_t)((uint64_t)((int64_t)s->aim_marker*0x780000)>>16));
        b=asr(bits((uint32_t)a+((uint32_t)a>>31)),1);
        o->y=bits(UINT32_C(0x5000)-(uint32_t)asr(bits((uint32_t)a+(uint32_t)b),8));
        if(o->animation==0xd9 && (s->weapon_flags&0x20000))
            (void)ssa_weapon_animation_set(o,0xda,5);
        else if(o->animation==0xda && !(s->weapon_flags&0x20000))
            (void)ssa_weapon_animation_set(o,0xd9,5);
        return 1;
    default:return 0;
    }
}
int ssa_hud_transient_step(SsaSpritePool *p,SsaWeaponObject *o,uint16_t flags)
{
    SsaSpriteDispatch *d;
    if(!p || p->loaded!=1 || p->updating || !(d=p->dispatch) || !o || o->type!=3 ||
       d->active_count>SSA_SPRITE_CAPACITY || p->free_count!=SSA_SPRITE_CAPACITY-d->active_count ||
       o->slot>=SSA_SPRITE_CAPACITY || d->objects[o->slot]!=o ||
       o->active_index>=d->active_count || d->active[o->active_index]!=o->slot)return -1;
    visibility(o,flags);
    return o->done?ssa_sprite_pool_release(p,o):1;
}
