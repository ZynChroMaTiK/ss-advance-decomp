/* Host presentation ownership. Original mover/enemy controllers stay intact. */
static unsigned audio_u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int audio_s16(const uint8_t *p){unsigned n=audio_u16(p);return n<32768?(int)n:(int)n-65536;}
static void audio_listener(SsaNativeGame *g){
    if(g->audio_ops.listen){const SsaBody *b=&g->players[0].entity.body;int32_t sine,cosine;
        SsaAudioListener listener={{b->x,b->y,g->player.camera.eye_z},0,0};
        ssa_sincos(&g->math,g->players[0].motion.movement.heading,&sine,&cosine);
        listener.right_x=cosine;listener.right_y=-sine;
        g->audio_ops.listen(g->audio_ops.context,&listener);
    }
}
static int sound(SsaNativeGame *g,unsigned id){
    if(g->audio_ops.source)g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_PLAY,id,1,NULL);
    else if(g->audio_ops.play)g->audio_ops.play(g->audio_ops.context,id);
    g->last_sound=id;return (int)++g->sound_serial;
}
static void stop_sound(SsaNativeGame *g,unsigned id){
    if(g->audio_ops.source)g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_STOP,id,1,NULL);
    else if(g->audio_ops.stop)g->audio_ops.stop(g->audio_ops.context,id);
    if(g->audio_weapon_loop==id)g->audio_weapon_loop=0;
}
static int weapon_sound(SsaNativeGame *g,unsigned id){
    /* Original wind-down replaces the shared hardware voice. Independent PC
     * effect instances need an explicit stop before playing that ending. */
    if(id==0x1a)stop_sound(g,0x19);
    else if(id==0x13)stop_sound(g,0x12);
    if(id==0x12 || id==0x16 || id==0x19){
        if(g->audio_weapon_loop && g->audio_weapon_loop!=id)stop_sound(g,g->audio_weapon_loop);
        g->audio_weapon_loop=id;
        if(g->audio_ops.source){
            g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_LOOP,id,1,NULL);
            g->last_sound=id;return (int)++g->sound_serial;
        }
    }
    return sound(g,id);
}
/* Boss battle19 and post-boss20 are gameplay, including the postlude's
 * automatic return to level music inside the mixer. */
static int audio_gameplay(const SsaNativeGame *g)
{return g->music_mode==2 || g->music_mode==19 || g->music_mode==20;}
static void audio_weapon_sync(SsaNativeGame *g){
    unsigned weapon=g->gameplay.weapon.current,id=0;
    if(g->audio_ops.play && audio_gameplay(g) && !(g->frame_game.flags&1) && g->input.focused && g->input.enabled &&
       g->input_runtime.controls_enabled && g->gameplay.life_game.input_state!=3 &&
       g->players[0].inventory.health>0 && !(g->players[0].motion.movement.flags&8) &&
       weapon==g->gameplay.weapon.requested && (g->gameplay.weapon.flags&0x400) &&
       (g->weapon.controller.held&1) && (weapon==1 || weapon==4 || weapon==5) &&
       g->players[0].inventory.ammo[weapon-1]!=0)
        id=weapon==1?0x12:weapon==4?0x16:0x19;
    if(g->audio_weapon_loop && g->audio_weapon_loop!=id)stop_sound(g,g->audio_weapon_loop);
    if(id && !g->audio_weapon_loop)(void)weapon_sound(g,id);
}
static SsaAudioPoint audio_entity_point(const SsaEntity *e){
    SsaAudioPoint p={e->body.x,e->body.y,e->body.z};
    if(e->type>=0xcd && e->type<=0xd8){int64_t z=(int64_t)p.z+e->body.height/2;
        p.z=z>INT32_MAX?INT32_MAX:z<INT32_MIN?INT32_MIN:(int32_t)z;}
    return p;
}
static void audio_retire(SsaNativeGame *g,unsigned slot){
    if(g->audio_entity_sources[slot] && g->audio_ops.source)
        g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_RETIRE,0,g->audio_entity_sources[slot],NULL);
    g->audio_entity_sources[slot]=0;g->audio_cd_loops[slot]=0;
}
static uint64_t audio_entity_source(SsaNativeGame *g,unsigned slot){
    SsaScriptActor *a=&g->spawn.actors[slot];
    /* State-zero entry after a previous lifetime, or a different type, retires
     * the old identity before a reused pool address can move its old sounds. */
    if(g->audio_entity_sources[slot] &&
       (g->audio_entity_types[slot]!=a->entity.type || (!a->activation.state && g->audio_entity_states[slot])))audio_retire(g,slot);
    if(!g->audio_entity_sources[slot])g->audio_entity_sources[slot]=128+ ++g->audio_source_serial;
    g->audio_entity_states[slot]=(uint8_t)a->activation.state;
    g->audio_entity_types[slot]=(uint8_t)a->entity.type;
    return g->audio_entity_sources[slot];
}
static uint64_t audio_source_for(SsaNativeGame *g,const SsaEntity *e){unsigned i;
    if(!e || e->type<0xcd || e->type>0xd8)return 0;
    for(i=0;i<100;++i)if(e==&g->spawn.actors[i].entity && g->spawn.actors[i].owned)return audio_entity_source(g,i);
    return 0;
}
static int world_sound(SsaNativeGame *g,unsigned id,const SsaEntity *source){
    SsaAudioPoint p;uint64_t identity;
    if(!source || source==&g->players[0].entity)return sound(g,id);
    audio_listener(g);identity=audio_source_for(g,source);p=audio_entity_point(source);
    if(g->audio_ops.source && identity){unsigned loop=id==0x3a || id==0x3d || id==0x4e || id==0x51 || id==0x5f;
        g->audio_ops.source(g->audio_ops.context,loop?SSA_AUDIO_SOURCE_LOOP:SSA_AUDIO_SOURCE_PLAY,id,identity,&p);}
    else if(g->audio_ops.play_at){
        /* Legacy sinks retain their original snapshot contract. */
        p=(SsaAudioPoint){source->body.x,source->body.y,source->body.z};
        g->audio_ops.play_at(g->audio_ops.context,id,&p);
    }else return sound(g,id);
    g->last_sound=id;return (int)++g->sound_serial;
}
static void world_stop(SsaNativeGame *g,unsigned id,const SsaEntity *source){
    uint64_t identity=audio_source_for(g,source);
    if(g->audio_ops.source && identity)g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_STOP,id,identity,NULL);
    else if(g->audio_ops.stop)g->audio_ops.stop(g->audio_ops.context,id);
}
static SsaAudioPoint audio_mover_point(const SsaNativeGame *g,unsigned index){
    const SsaMapAssets *m=&g->map;unsigned i,j,sector=audio_u16(g->level_runtime.movers.records[index]),count=0;
    const uint8_t *s=m->data+m->sectors+sector*20;int64_t x=0,y=0;
    SsaAudioPoint p={0,0,0};
    for(i=0;i<m->line_count;++i){const uint8_t *l=m->data+m->lines+i*40;
        if(audio_u16(l+12)!=sector && (!(audio_u16(l+26)&4) || audio_u16(l+14)!=sector))continue;
        for(j=0;j<2;++j){const uint8_t *v=m->data+m->vertices+audio_u16(l+j*2)*4;
            x+=audio_s16(v);y+=audio_s16(v+2);++count;}
    }
    if(count){p.x=(int32_t)(x*65536/count);p.y=(int32_t)(y*65536/count);}
    p.z=(int32_t)(((int64_t)audio_s16(s)+audio_s16(s+2))*32768);return p;
}
static void audio_movers_sync(SsaNativeGame *g,unsigned end_cue){unsigned i,moving=0,stopped=0;
    for(i=0;i<g->level_runtime.movers.count;++i){
        unsigned active=(audio_u16(g->level_runtime.movers.records[i]+14)&8)!=0 && audio_gameplay(g);
        uint64_t source=2+i;SsaAudioPoint p=audio_mover_point(g,i);
        if(active){
            if(g->audio_ops.source)g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_LOOP,0x67,source,&p);
            else if(!g->audio_movers[i]){
                if(g->audio_ops.play_at)g->audio_ops.play_at(g->audio_ops.context,0x67,&p);
                else if(g->audio_ops.play)g->audio_ops.play(g->audio_ops.context,0x67);}
            if(!g->audio_movers[i]){g->last_sound=0x67;++g->sound_serial;}
            ++moving;
        }else if(g->audio_movers[i]){
            if(g->audio_ops.source)g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_STOP,0x67,source,NULL);
            ++stopped;
            if(end_cue && audio_gameplay(g)){
                if(g->audio_ops.source)g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_PLAY,0x68,source,&p);
                else if(g->audio_ops.play_at)g->audio_ops.play_at(g->audio_ops.context,0x68,&p);
                else if(g->audio_ops.play)g->audio_ops.play(g->audio_ops.context,0x68);
                g->last_sound=0x68;++g->sound_serial;
            }
        }
        g->audio_movers[i]=(uint8_t)active;
    }
    if(stopped && !moving && !g->audio_ops.source && g->audio_ops.stop)g->audio_ops.stop(g->audio_ops.context,0x67);
}
static int mover_sound(void *p,unsigned id){SsaNativeGame *g=p;
    if(id==0x67 || id==0x68){audio_listener(g);audio_movers_sync(g,id==0x68);}
    else sound(g,id);
    return 0;
}
static void audio_world_sync(SsaNativeGame *g){unsigned i;
    audio_weapon_sync(g);
    audio_movers_sync(g,1);
    for(i=0;i<100;++i){SsaScriptActor *a=&g->spawn.actors[i];
        if(!a->owned || a->entity.type<0xcd || a->entity.type>0xd8){audio_retire(g,i);continue;}
        if(g->audio_ops.source){uint64_t source=audio_entity_source(g,i);SsaAudioPoint p=audio_entity_point(&a->entity);
            SsaEnemyCdRuntime *cd=g->entities.enemies[i];unsigned looping=0,song=a->entity.type==0xce?0x4e:0x3a;
            g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_MOVE,0,source,&p);
            if(cd && cd->active && !cd->controller.destroy_requested && audio_gameplay(g)){
                int distance=cd->navigation.distance<32768?(int)cd->navigation.distance:(int)cd->navigation.distance-65536;
                looping=cd->navigation.phase==2 && distance && distance<=(g->audio_cd_loops[i]?1024:1023) && a->activation.state!=2;
            }
            if(a->entity.type==0xce && g->enemies.active[i] && !g->enemies.controllers[i].destroy_requested && audio_gameplay(g)){
                const SsaEnemyNavigation *n=&g->enemies.navigation[i];
                int distance=n->distance<32768?(int)n->distance:(int)n->distance-65536;
                looping=n->phase==2 && distance && distance<=(g->audio_cd_loops[i]?400:399) && a->activation.state!=3;
            }
            if(looping){g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_LOOP,song,source,&p);
                if(!g->audio_cd_loops[i]){g->last_sound=song;++g->sound_serial;}}
            else if(g->audio_cd_loops[i])g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_STOP,song,source,NULL);
            g->audio_cd_loops[i]=(uint8_t)looping;
        }
    }
    if(!g->audio_ops.source && g->audio_ops.move && g->game.cd_game.loop_source){const SsaBody *b=&g->game.cd_game.loop_source->body;
        SsaAudioPoint p={b->x,b->y,b->z};g->audio_ops.move(g->audio_ops.context,0x3a,&p);}
}
static void audio_unbind(SsaNativeGame *g){unsigned i;
    if(g->audio_weapon_loop)stop_sound(g,g->audio_weapon_loop);
    if(g->audio_ops.source){g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_STOP,SSA_AUDIO_SONGS,1,NULL);
        for(i=0;i<25;++i)g->audio_ops.source(g->audio_ops.context,SSA_AUDIO_SOURCE_STOP,SSA_AUDIO_SONGS,2+i,NULL);}
    for(i=0;i<100;++i)audio_retire(g,i);
    memset(g->audio_movers,0,sizeof(g->audio_movers));
}
