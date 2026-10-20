static unsigned world_audio_cases;
static unsigned source_instances(const SsaAudio *a,uint64_t source,unsigned song){unsigned i,count=0;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)count+=a->players[i].source==source && a->players[i].song==song && a->players[i].active;
    return count;
}
static unsigned source_slot(const SsaAudio *a,uint64_t source,unsigned song){unsigned i;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(a->players[i].source==source && a->players[i].song==song && a->players[i].active)return i;
    CHECK(0);return 0;
}
static void source_fixture(const uint8_t *rom){
    SsaAudio a={0},b={0};SsaAudioListener ear={{0,0,0},65536,0};
    SsaAudioPoint left={-256*65536,0,0},right={256*65536,0,0},far={-1024*65536,0,0};
    unsigned l,r,shot;uint64_t notes,clock;uint32_t pc;
    CHECK(ssa_audio_init(&a,rom,0x800000,24000)==1 && ssa_audio_init(&b,rom,0x800000,24000)==1);
    CHECK(ssa_audio_level_music(&a,0)==1 && ssa_audio_level_music(&b,0)==1 && ssa_audio_listen(&a,&ear)==1);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_LOOP,0x12,101,&left)==1);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_LOOP,0x12,102,&right)==1);
    l=source_slot(&a,101,0x12);r=source_slot(&a,102,0x12);
    CHECK(l!=r && a.players[l].left>a.players[l].right*3 && a.players[r].right>a.players[r].left*3);
    notes=a.notes;clock=a.players[l].clock;pc=a.players[l].tracks[0].pc;
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_LOOP,0x12,101,&far)==1);
    CHECK(source_instances(&a,101,0x12)==1 && a.notes==notes && a.players[l].clock==clock && a.players[l].tracks[0].pc==pc);
    CHECK(a.players[l].target_left<a.players[r].target_right/2);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_PLAY,0x12,101,&left)==1);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_PLAY,0x12,101,&left)==1 && source_instances(&a,101,0x12)==3);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_PLAY,0x12,1,NULL)==1);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_MOVE,0,101,&far)==3);
    CHECK(!memcmp(&a.players[r].position,&right,sizeof(right)));
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_STOP,0x12,1,NULL)==1 && source_instances(&a,102,0x12)==1 && source_instances(&a,101,0x12)==3);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_RETIRE,0,101,NULL)==3);
    CHECK(!a.players[l].active && source_instances(&a,102,0x12)==1);
    shot=0;
    for(l=1;l<SSA_AUDIO_PLAYERS;++l)if(a.players[l].active && !a.players[l].source){
        CHECK(!memcmp(&a.players[l].position,&far,sizeof(far)));++shot;}
    CHECK(shot==2 && ssa_audio_source(&a,SSA_AUDIO_SOURCE_MOVE,0,101,&right)==0);
    mix_frames(&a,400);mix_frames(&b,400);same_music(&a,&b);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_STOP,SSA_AUDIO_SONGS,102,NULL)==1 && !a.players[r].active);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_LOOP,0x73,103,&left)==-1);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_PLAY,0x12,0,&left)==-1);
    CHECK(ssa_audio_source(&a,SSA_AUDIO_SOURCE_MOVE,0,103,NULL)==-1);
    CHECK(ssa_audio_source(&a,(SsaAudioSourceCommand)99,0,103,NULL)==-1);
    ssa_audio_reset(&a);CHECK(!source_instances(&a,102,0x12));++world_audio_cases;
}
static void scene_source(void *context,SsaAudioSourceCommand command,unsigned id,uint64_t source,const SsaAudioPoint *point){
    SceneAudio *s=context;
    if(command==SSA_AUDIO_SOURCE_PLAY || command==SSA_AUDIO_SOURCE_LOOP ||
       command==SSA_AUDIO_SOURCE_ENEMY_PLAY || command==SSA_AUDIO_SOURCE_ENEMY_LOOP){s->last_song=id;
        if(point){++s->world;s->point=*point;}else ++s->local;}
    if(command==SSA_AUDIO_SOURCE_MOVE)++s->moves;
    CHECK(ssa_audio_source(s->mixer,command,id,source,point)>=0);
}
/* Inject observer positions while exercising the actual door/pickup sinks. */
static void distance_listen(void *context,const SsaAudioListener *unused){SceneAudio *s=context;
    (void)unused;CHECK(ssa_audio_listen(s->mixer,&s->ear)==1);
}
static void distance_emit(SsaNativeGame *g,unsigned kind,unsigned mover){
    if(!kind)CHECK(ssa_sector_movers_command(&g->level_runtime.movers,mover,SSA_SECTOR_MOVER_OPEN,&g->mover_ops)==1);
    else if(kind==1){
        g->level_runtime.movers.records[mover][14]&=(uint8_t)~8u;g->audio_movers[mover]=1;
        CHECK(g->mover_ops.sound(g->mover_ops.context,0x68)==0);
    }else g->touch_ops.event(g->touch_ops.context,&g->pickup,0,SSA_PICKUP_SOUND,0x35,0);
}
static unsigned distance_peak(SsaAudio *a){int16_t out[4096];unsigned i,peak=0;
    CHECK(ssa_audio_render(a,out,2048)==1);
    for(i=0;i<4096;++i){unsigned n=(unsigned)abs(out[i]);if(n>peak)peak=n;}
    return peak;
}
static void world_distance_fixture(const uint8_t *rom){
    static const unsigned distances[]={0,32,64,128,256,512,1024,2048,4096};
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaAudio a={0};SceneAudio scene={0};unsigned kind,i,slot;
    SsaNativeAudioOps ops={scene_play,scene_stop,&scene,scene_world,distance_listen,scene_move,scene_music,scene_source};
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1 && ssa_audio_init(&a,rom,0x800000,24000)==1);
    scene.mixer=&a;scene.ear.right_x=65536;CHECK(ssa_native_game_bind_audio(g,&ops)==1);
    for(slot=0;slot<100;++slot)if(g->spawn.actors[slot].owned && g->spawn.actors[slot].entity.type==0xe1)break;
    CHECK(slot<100);g->pickup.slot=slot;
    for(kind=0;kind<2;++kind){SsaAudioPoint source;unsigned near,previous=0;
        ssa_audio_reset(&a);distance_emit(g,kind,0);source=scene.point;
        for(i=0;i<sizeof(distances)/sizeof(distances[0]);++i){unsigned peak;
            ssa_audio_reset(&a);scene.ear.position=source;
            scene.ear.position.y-=(int32_t)(distances[i]*65536u);
            distance_emit(g,kind,0);peak=distance_peak(&a);
            CHECK(!memcmp(&scene.point,&source,sizeof(source)) && peak>0);
            if(i)CHECK(peak<previous);else previous=peak;
            if(!i)near=peak;
            previous=peak;
            if(distances[i]==256)CHECK(abs((int)peak*2-(int)near)<=2);
            if(distances[i]==1024)CHECK(abs((int)peak*5-(int)near)<=5);
        }
        /* Vertical distance uses the same volume law; side sources retain
         * stereo direction. No sample phase/envelope change between cases. */
        ssa_audio_reset(&a);scene.ear.position=source;scene.ear.position.z-=256*65536;
        distance_emit(g,kind,0);CHECK(abs((int)distance_peak(&a)*2-(int)near)<=2);
        ssa_audio_reset(&a);scene.ear.position=source;scene.ear.position.x-=256*65536;
        distance_emit(g,kind,0);
        {unsigned p=latest(&a,kind==0?0x67:0x68);
            CHECK(a.players[p].positioned && a.players[p].right>a.players[p].left*3);}
    }
    /* Pickups use the same local path as player sounds, on both source-aware
     * and legacy sinks. Their PCM must match local playback regardless of
     * item/listener position or listener yaw. Preserve original track pan. */
    {int16_t reference[4096],out[4096];unsigned legacy;
        ssa_audio_reset(&a);CHECK(ssa_audio_play(&a,0x35)==1);
        CHECK(ssa_audio_render(&a,reference,2048)==1);
        for(legacy=0;legacy<2;++legacy){
            ops.source=legacy?NULL:scene_source;CHECK(ssa_native_game_bind_audio(g,&ops)==1);
            for(i=0;i<sizeof(distances)/sizeof(distances[0]);++i){unsigned local=scene.local,world=scene.world,p;
                ssa_audio_reset(&a);
                scene.ear.position=(SsaAudioPoint){(int32_t)(distances[i]*65536u),-256*65536,512*65536};
                scene.ear.right_x=i%2?65536:0;scene.ear.right_y=i%2?0:-65536;
                CHECK(ssa_audio_listen(&a,&scene.ear)==1);
                g->spawn.actors[slot].entity.body.z+=(int32_t)65536;
                distance_emit(g,2,0);p=latest(&a,0x35);
                CHECK(scene.local==local+1 && scene.world==world && !a.players[p].positioned);
                CHECK(ssa_audio_render(&a,out,2048)==1 && !memcmp(out,reference,sizeof(out)));
            }
        }
    }
    /* Every enemy type reaches the extended-range path through the actual
     * presentation bridge, including bosses and finite hurt/death sounds. */
    {SsaScriptActor *actor=&g->spawn.actors[slot];unsigned type=actor->entity.type;
        SsaEnemyControl control={0};SsaEnemyControlEvent event={0};
        ops.source=scene_source;CHECK(ssa_native_game_bind_audio(g,&ops)==1);
        control.activation=&actor->activation;event.kind=SSA_ENEMY_CONTROL_SOUND;event.args[0]=0x43;
        for(i=0xcd;i<=0xd8;++i){unsigned p;
            ssa_audio_reset(&a);actor->entity.type=(uint16_t)i;
            CHECK(g->enemy_ops.presentation(g->enemy_ops.context,&control,&event)==0);
            p=latest(&a,0x43);CHECK(a.players[p].positioned && a.players[p].enemy_range && a.players[p].source);
        }
        actor->entity.type=(uint16_t)type;
    }
    ++world_audio_cases;ssa_native_game_free(g);free(g);
}
/* Same bridge as level_runtime.c's player_line -> player_sound_id path. */
static int player_door_sound(void *context,unsigned id){SsaNativeGame *g=context;
    return g->player_ops.sound(g->player_ops.context,&g->player,id);
}
static void player_door_fixture(const uint8_t *rom){
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaAudio a={0};SceneAudio scene={0};
    SsaNativeAudioOps ops={scene_play,scene_stop,&scene,scene_world,scene_listen,scene_move,scene_music,scene_source};
    SsaSectorMoverOps bridge={player_door_sound,g};unsigned i,j,mover=0,line=0,cycle;int16_t out[1600];
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1 && ssa_audio_init(&a,rom,0x800000,24000)==1);
    scene.mixer=&a;CHECK(ssa_native_game_bind_audio(g,&ops)==1);
    for(i=0;i<g->level_runtime.movers.count;++i){const uint8_t *r=g->level_runtime.movers.records[i];
        unsigned flags=r[14]|(unsigned)r[15]<<8,sector=r[0]|(unsigned)r[1]<<8;
        if(flags&(1|4|256))continue;
        for(j=0;j<g->map.line_count;++j){const uint8_t *l=g->map.data+g->map.lines+j*40;
            if((l[12]|(unsigned)l[13]<<8)==sector || ((l[26]&4) && (l[14]|(unsigned)l[15]<<8)==sector)){mover=i;line=j;break;}}
        if(j<g->map.line_count)break;
    }
    CHECK(i<g->level_runtime.movers.count);
    for(cycle=0;cycle<3;++cycle){unsigned energy=0;
        CHECK(ssa_level_runtime_line(&g->level_runtime,1,line,&bridge)==0);
        CHECK(source_instances(&a,2+mover,0x67)==1 && !source_instances(&a,1,0x67));
        for(i=0;i<120 && (g->level_runtime.movers.records[mover][14]&8);++i){
            CHECK(ssa_sector_movers_step(&g->level_runtime.movers,1,&g->mover_ops)==1 && ssa_audio_render(&a,out,400)==1);
            for(j=0;j<800;++j)energy|=out[j]!=0;
        }
        CHECK(i<120 && energy);
        /* Let the finite finish sample end, then require actual PCM silence.
         * A player-owned tied67 voice survived here before the fix. */
        mix_frames(&a,72000);CHECK(ssa_audio_render(&a,out,800)==1);
        for(j=0;j<1600;++j)CHECK(out[j]==0);
        for(j=1;j<SSA_AUDIO_PLAYERS;++j)CHECK(!a.players[j].active || a.players[j].song!=0x67);
        for(i=0;i<30;++i)CHECK(ssa_level_runtime_line(&g->level_runtime,1,line,&bridge)==0);
        CHECK(ssa_audio_render(&a,out,800)==1);for(j=0;j<1600;++j)CHECK(out[j]==0);
        CHECK(ssa_sector_movers_command(&g->level_runtime.movers,mover,SSA_SECTOR_MOVER_CLOSE,&g->mover_ops)==1);
        for(i=0;i<120 && (g->level_runtime.movers.records[mover][14]&8);++i){
            CHECK(ssa_sector_movers_step(&g->level_runtime.movers,1,&g->mover_ops)==1);mix_frames(&a,400);}
        CHECK(i<120);mix_frames(&a,72000);
    }
    ++world_audio_cases;ssa_native_game_free(g);free(g);
}
static void world_fixture(const uint8_t *rom){
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaAudio a={0};SceneAudio scene={0};unsigned i,j,slots[2],n=0;
    SsaNativeAudioOps ops={scene_play,scene_stop,&scene,scene_world,scene_listen,scene_move,scene_music,scene_source};
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1 && ssa_audio_init(&a,rom,0x800000,24000)==1);
    scene.mixer=&a;CHECK(ssa_audio_level_music(&a,0)==1 && ssa_native_game_bind_audio(g,&ops)==1);
    CHECK(g->level_runtime.movers.count>=2);
    /* Real mover command/step callbacks: opening, stopping independently,
     * closing, reversing and repeated commands never accumulate loop copies. */
    for(i=0;i<2;++i){CHECK(ssa_sector_movers_command(&g->level_runtime.movers,i,SSA_SECTOR_MOVER_OPEN,&g->mover_ops)==1);
        CHECK(source_instances(&a,2+i,0x67)==1);}
    CHECK(ssa_sector_movers_command(&g->level_runtime.movers,0,SSA_SECTOR_MOVER_OPEN,&g->mover_ops)==1 && source_instances(&a,2,0x67)==1);
    /* Give the second mover a longer trip, verifying one stop cannot cut it. */
    {uint8_t *r=g->level_runtime.movers.records[1];unsigned sector=r[0]|(unsigned)r[1]<<8;
        uint8_t *s=g->map.data+g->map.sectors+sector*20;
        if(!(r[15]&1)){s[2]=s[0];s[3]=s[1];}
        r[12]=1;r[13]=0;
    }
    for(i=0;i<300 && source_instances(&a,2,0x67);++i)
        CHECK(ssa_sector_movers_step(&g->level_runtime.movers,1,&g->mover_ops)==1);
    CHECK(i<300 && !source_instances(&a,2,0x67) && source_instances(&a,3,0x67)==1 && source_instances(&a,2,0x68)==1);
    for(i=0;i<400 && source_instances(&a,3,0x67);++i)
        CHECK(ssa_sector_movers_step(&g->level_runtime.movers,1,&g->mover_ops)==1);
    CHECK(i<400 && !source_instances(&a,3,0x67));
    for(j=0;j<3;++j){
        CHECK(ssa_sector_movers_command(&g->level_runtime.movers,0,j==1?SSA_SECTOR_MOVER_OPEN:SSA_SECTOR_MOVER_CLOSE,&g->mover_ops)==1);
        CHECK(source_instances(&a,2,0x67)==1);
    }
    for(i=0;i<300 && source_instances(&a,2,0x67);++i)CHECK(ssa_sector_movers_step(&g->level_runtime.movers,1,&g->mover_ops)==1);
    CHECK(i<300 && !source_instances(&a,2,0x67) && a.players[0].active);++world_audio_cases;
    /* Controlled stationary simulation checks presentation for two CD actors
     * simultaneously. Controller/navigation gameplay is validated separately. */
    for(i=0;i<100 && n<2;++i)if(g->spawn.actors[i].owned)slots[n++]=i;
    CHECK(n==2);g->frame_game.flags|=1;
    {SsaEnemyCdRuntime cds[2]={{0}};SsaScriptActor saved[2];uint64_t identities[2];
        for(i=0;i<2;++i){SsaScriptActor *actor=&g->spawn.actors[slots[i]];saved[i]=*actor;
            actor->entity.type=0xcd;actor->activation.state=1;actor->entity.body.height=120*65536;
            cds[i].active=1;cds[i].navigation.phase=2;cds[i].navigation.distance=4095;
            g->entities.enemies[slots[i]]=&cds[i];}
        CHECK(ssa_native_game_tick(g)==1);
        for(i=0;i<2;++i){identities[i]=g->audio_entity_sources[slots[i]];
            CHECK(identities[i] && source_instances(&a,identities[i],0x3a)==1);}
        CHECK(identities[0]!=identities[1]);
        cds[0].navigation.distance=4096;CHECK(ssa_native_game_tick(g)==1 && source_instances(&a,identities[0],0x3a)==1);
        cds[0].navigation.distance=4097;CHECK(ssa_native_game_tick(g)==1 && !source_instances(&a,identities[0],0x3a));
        cds[0].navigation.distance=4096;CHECK(ssa_native_game_tick(g)==1 && !source_instances(&a,identities[0],0x3a));
        cds[0].navigation.distance=4095;CHECK(ssa_native_game_tick(g)==1 && source_instances(&a,identities[0],0x3a)==1);
        cds[0].navigation.phase=0;CHECK(ssa_native_game_tick(g)==1);
        CHECK(!source_instances(&a,identities[0],0x3a) && source_instances(&a,identities[1],0x3a)==1);
        /* Same-ID one-shots follow their own enemy; retirement freezes the
         * final one-shot position and terminates its loop, leaving others. */
        {SsaEnemyControl control={0};SsaEnemyControlEvent event={0};SsaAudioPoint old;
            control.activation=&g->spawn.actors[slots[1]].activation;
            event.kind=SSA_ENEMY_CONTROL_SOUND;event.args[0]=0x43;
            CHECK(g->enemy_ops.presentation(g->enemy_ops.context,&control,&event)==0);
            n=source_slot(&a,identities[1],0x43);old=a.players[n].position;
            g->spawn.actors[slots[1]].entity.body.z+=65536;
            CHECK(ssa_native_game_tick(g)==1 && a.players[n].position.z==old.z+65536);
            g->spawn.actors[slots[1]].owned=0;CHECK(ssa_native_game_tick(g)==1);
            CHECK(!source_instances(&a,identities[1],0x3a) && a.players[n].active && !a.players[n].source);
        }
        for(i=0;i<2;++i){g->entities.enemies[slots[i]]=NULL;g->spawn.actors[slots[i]]=saved[i];}
    }
    ++world_audio_cases;
    /* CE retains independent emitters and one-unit hysteresis at the new
     *1599 start /1600 stop distance. Death must still stop its own loop. */
    {SsaScriptActor saved[2];SsaEnemyNavigation nav[2];unsigned active[2];uint64_t identities[2];
        for(i=0;i<2;++i){SsaScriptActor *actor=&g->spawn.actors[slots[i]];saved[i]=*actor;
            actor->entity.type=0xce;actor->activation.state=1;
            nav[i]=g->enemies.navigation[slots[i]];active[i]=g->enemies.active[slots[i]];
            g->enemies.active[slots[i]]=1;g->enemies.navigation[slots[i]].phase=2;g->enemies.navigation[slots[i]].distance=1599;}
        CHECK(ssa_native_game_tick(g)==1);
        for(i=0;i<2;++i){identities[i]=g->audio_entity_sources[slots[i]];CHECK(source_instances(&a,identities[i],0x4e)==1);}
        g->enemies.navigation[slots[0]].distance=1600;g->spawn.actors[slots[1]].activation.state=3;
        CHECK(ssa_native_game_tick(g)==1 && source_instances(&a,identities[0],0x4e)==1 && !source_instances(&a,identities[1],0x4e));
        g->enemies.navigation[slots[0]].distance=1601;CHECK(ssa_native_game_tick(g)==1 && !source_instances(&a,identities[0],0x4e));
        g->enemies.navigation[slots[0]].distance=1600;CHECK(ssa_native_game_tick(g)==1 && !source_instances(&a,identities[0],0x4e));
        g->enemies.navigation[slots[0]].distance=1599;CHECK(ssa_native_game_tick(g)==1 && source_instances(&a,identities[0],0x4e)==1);
        for(i=0;i<2;++i){g->enemies.active[slots[i]]=(uint8_t)active[i];g->enemies.navigation[slots[i]]=nav[i];g->spawn.actors[slots[i]]=saved[i];}
    }
    ++world_audio_cases;
    CHECK(ssa_native_game_bind_audio(g,NULL)==1);
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)CHECK(!a.players[i].source_loop);
    ssa_native_game_free(g);free(g);
}
