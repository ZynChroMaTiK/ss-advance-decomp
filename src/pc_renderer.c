#include "ssa/pc_renderer.h"
#include "ssa/pc_scene.h"
#include "ssa/native_game.h"
static uint32_t rgb(unsigned n){unsigned r=n&31,g=(n>>5)&31,b=(n>>10)&31;
 return (((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));}
static int edge(double y,unsigned height){int n;if(y<=0.5)return 0;if(y>=(double)height+0.5)return (int)height;
 y-=0.5;n=(int)y;return n+(y>n);}
enum {CLASSIC_CENTER,CLASSIC_SCORE,CLASSIC_INBOX,CLASSIC_BOSS,CLASSIC_ANALYSIS,
      CLASSIC_STATUS,CLASSIC_WEAPON,CLASSIC_AMMO,CLASSIC_SELECTOR,CLASSIC_PICKUP,CLASSIC_HEADER};
static unsigned classic_region(unsigned x,unsigned y,unsigned ammo_left,int inbox_x,int inbox_y,unsigned boss)
{
    if((int)x>=inbox_x && (int)x<inbox_x+24 && (int)y>=inbox_y && (int)y<inbox_y+16)return CLASSIC_INBOX;
    if(y<22)return x<64?CLASSIC_SCORE:CLASSIC_HEADER;
    if(boss && y>=26 && y<36)return CLASSIC_BOSS;
    if(y>=(boss?40u:28u) && y<(boss?56u:44u))return CLASSIC_ANALYSIS;
    if(y>=103 && y<120)return CLASSIC_SELECTOR;
    if(y>=120 && x<50)return CLASSIC_STATUS;
    if(y>=140)return x>=ammo_left?CLASSIC_AMMO:CLASSIC_WEAPON;
    if(y>=126 && y<136)return CLASSIC_PICKUP;
    return CLASSIC_CENTER;
}
static int hud(SsaPcRenderer *r,const SsaNativeGame *g)
{
    uint8_t zero[14400]={0};uint16_t black[256]={0},white[256],a[38400],b[38400];unsigned i,x,y;
    double scale=r->width/240.0<r->height/160.0?r->width/240.0:r->height/160.0;
    double left=(r->width-240*scale)/2,top=(r->height-160*scale)/2;
    unsigned pass,groups=g->hud_style==SSA_HUD_ORIGINAL && !g->hud_suppressed && g->oam.count?6:1;
    unsigned separate=g->pc_weapon_separate && (g->weapon_bob_x || g->weapon_bob_y) && (left>0.001 || top>0.001);
    for(i=0;i<256;++i)white[i]=32767;
    for(pass=0;pass<groups+separate;++pass){
    unsigned weapon=separate && !pass,group=weapon?0:pass-separate;
    const SsaSpriteOam *o=weapon?&g->pc_weapon_oam:groups==6?&g->original_hud_oam[group]:&g->oam;
    double ox=left,oy=top;
    if(separate && !weapon && !group)o=groups==6?&g->pc_center_hud_oam:&g->pc_hud_oam;
    if(weapon){ox+=(g->weapon_bob_x/256)*scale;oy+=(g->weapon_bob_y/256)*scale;}
    if(!o->count)continue;
    /* Original GBA anchors and padding: score/clock centered above, lives
     * upper-left, weapon upper-right, status lower-left, ammo lower-right. */
    if(group==2 || group==3)ox=0;
    if(group==4 || group==5)ox=left*2;
    if(group==1 || group==2 || group==4)oy=0;
    if(group==3 || group==5)oy=top*2;
    if(ssa_display_compose_gameplay(&g->registers,zero,sizeof(zero),black,g->sprite_assets.palette,o,g->cache.tiles,sizeof(g->cache.tiles),a,38400,g->players[0].inventory.health,g->players[0].inventory.stat_c4)<0 ||
       ssa_display_compose_gameplay(&g->registers,zero,sizeof(zero),white,g->sprite_assets.palette,o,g->cache.tiles,sizeof(g->cache.tiles),b,38400,g->players[0].inventory.health,g->players[0].inventory.stat_c4)<0)return -1;
    /* Expand only opaque native HUD runs, instead of testing and converting
     * every world pixel. Pixel-center coverage matches the old sampler. */
    for(y=0;y<160;++y){int yy,first=edge(oy+y*scale,r->height),last=edge(oy+(y+1)*scale,r->height);
        for(x=0;x<240;){unsigned at=y*240+x,end=x+1;int xx,l,h;uint32_t color;
            if(a[at]!=b[at]){++x;continue;}
            while(end<240 && a[y*240+end]==a[at] && a[y*240+end]==b[y*240+end])++end;
            l=edge(ox+x*scale,r->width);h=edge(ox+end*scale,r->width);color=rgb(a[at]);
            for(yy=first;yy<last;++yy)for(xx=l;xx<h;++xx)r->pixels[(size_t)yy*r->width+(unsigned)xx]=color;
            x=end;}}}
    /* Classic backings sit above the world/weapon layer and below the indexed
     * HUD art. Blend at the actual output resolution; no world resampling. */
    if(g->hud_style==SSA_HUD_CLASSIC && !g->hud_suppressed){uint8_t canvas[38400],background[38400];
        unsigned count=0,ammo_left,boss=g->hud.boss_present && g->hud.boss_health>0,attention;
        int inbox_x,inbox_y;double origins_x[11],origins_y[11],weapon_x;
        if(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)!=1)return -1;
        if(ssa_classic_hud_inbox_position(g->players[0].inventory.health>0 && !g->finished?g->hud_netricsa_ticks:0,
            boss,&inbox_x,&inbox_y,&attention)!=1)return -1;
        for(i=2;i<10;++i)if(g->players[0].inventory.ammo[i]>0)++count;
        ammo_left=239-16*count;
        weapon_x=(g->gameplay.weapon.requested>2?66.0:77.0)*r->width/240.0;
        if(g->gameplay.weapon.requested>2 && count){double limit=r->width-(240-ammo_left+53)*scale;
            if(weapon_x>limit)weapon_x=limit;}
        for(i=0;i<11;++i){origins_x[i]=left;origins_y[i]=top;}
        origins_x[CLASSIC_SCORE]=origins_x[CLASSIC_STATUS]=0;
        origins_x[CLASSIC_AMMO]=left*2;
        origins_x[CLASSIC_INBOX]=left*(2.0-attention/30.0);
        origins_x[CLASSIC_WEAPON]=weapon_x-(g->gameplay.weapon.requested>2?(count==8?58:66):77)*scale;
        origins_y[CLASSIC_SCORE]=origins_y[CLASSIC_HEADER]=origins_y[CLASSIC_BOSS]=0;
        origins_y[CLASSIC_STATUS]=origins_y[CLASSIC_WEAPON]=origins_y[CLASSIC_AMMO]=top*2;
        origins_y[CLASSIC_ANALYSIS]=top*0.4;
        origins_y[CLASSIC_INBOX]=origins_y[CLASSIC_ANALYSIS]*attention/30.0;
        origins_y[CLASSIC_SELECTOR]=top*1.39375;
        origins_y[CLASSIC_PICKUP]=top*1.64;
        for(y=0;y<160;++y){
            for(x=0;x<240;){unsigned at=y*240+x,end=x+1,index=canvas[at];int xx,l,h;uint32_t color;
                unsigned region;int yy,first,last;
                if(!index && !background[at]){++x;continue;}
                region=classic_region(x,y,ammo_left,inbox_x,inbox_y,boss);
                while(end<240 && canvas[y*240+end]==index && background[y*240+end]==background[at] &&
                      classic_region(end,y,ammo_left,inbox_x,inbox_y,boss)==region)++end;
                l=edge(origins_x[region]+x*scale,r->width);h=edge(origins_x[region]+end*scale,r->width);
                first=edge(origins_y[region]+y*scale,r->height);last=edge(origins_y[region]+(y+1)*scale,r->height);
                color=index?rgb(g->sprite_assets.palette[index]):0;
                for(yy=first;yy<last;++yy)for(xx=l;xx<h;++xx){size_t pixel=(size_t)yy*r->width+(unsigned)xx;
                    r->pixels[pixel]=index?ssa_classic_hud_blend888(color,r->pixels[pixel],
                        ssa_classic_hud_alpha(x,y,g->hud_netricsa_ticks,boss)):ssa_classic_hud_darken888(r->pixels[pixel]);}
                x=end;}}
    }
    return 1;
}
int ssa_pc_renderer_draw(SsaPcRenderer *r,const SsaNativeGame *g)
{
    int result=ssa_pc_scene_draw(r,g);
    if(result<0)return result;
    r->updating=1;result=hud(r,g);r->updating=0;return result;
}