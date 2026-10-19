/* Screen entry/exit and NETRICSA controller. App owns the shared resources;
 * these transitions run on the UI thread, outside simulation updates. */
static int begin_map(App *a)
{
    if(!a->map_view){a->map_view=calloc(1,sizeof(*a->map_view));if(!a->map_view)return 0;}
    else memset(a->map_view,0,sizeof(*a->map_view));
    if(ssa_map_view_init(a->map_view,a->rom,a->rom_size,a->level,a->language,a->difficulty)!=1)return 0;
    if(a->audio.device && !ssa_win32_audio_restart(&a->audio,a->level)){
        fprintf(stderr,"Map audio start failed (%u); continuing silently.\n",a->audio.last_error);
        ssa_win32_audio_close(&a->audio);(void)ssa_native_game_bind_audio(a->game,NULL);
    }
    a->map_active=1;a->map_fire=0;a->map_black_hold=MAP_BLACK_HOLD_TICKS;a->completion_recorded=a->tally_recorded=0;
    a->black_presented_at.QuadPart=a->reveal_presented_at.QuadPart=0;
    a->level_revealing=a->level_fade=0;++a->map_views;return render(a);
}
static int begin_netricsa(App *a,unsigned prelevel)
{
    SsaLevelStatistics current;SsaNetricsaData view={0};SsaNetricsaJournal journal;unsigned i;
    if(!prelevel && !a->settings.ingame_netricsa)return 0;
    if(a->death_active || a->game->players[0].inventory.health<=0 || a->screen_active || a->net_active || a->map_active || a->menu_active || a->story_active || a->level_revealing ||
       ssa_native_game_statistics(&current,a->game)!=1)return 0;
    if(!a->netricsa){a->netricsa=calloc(1,sizeof(*a->netricsa));if(!a->netricsa)return 0;}
    else memset(a->netricsa,0,sizeof(*a->netricsa));
    view.rows=current.rows;view.totals=a->statistics;
    if(ssa_statistics_complete(&view.totals,&current)!=1)return 0;
    for(i=0;i<12;++i){view.rows.enemies[i].spawned+=a->statistics_rows.enemies[i].spawned;
        view.rows.enemies[i].hits+=a->statistics_rows.enemies[i].hits;view.rows.enemies[i].kills+=a->statistics_rows.enemies[i].kills;}
    for(i=0;i<10;++i){view.rows.weapons[i].shots+=a->statistics_rows.weapons[i].shots;
        view.rows.weapons[i].hits+=a->statistics_rows.weapons[i].hits;}
    view.level=a->level;view.lives=prelevel?a->game->gameplay.life_game.continue_flag:a->game->game.life_game.continue_flag;
    view.random=a->game->frame_game.counter*0x41c64e6du+12345u;
    if(ssa_native_game_netricsa_entries(&journal,a->game)!=1)return 0;
    view.weapon_entries=journal.weapons;view.enemy_entries=journal.enemies;
    if(!prelevel){int latest=ssa_native_game_netricsa_latest(a->game);if(latest<0)return 0;
        view.opening_entry=(unsigned)latest;}
    if(ssa_netricsa_init(a->netricsa,a->rom,a->rom_size,a->language,&view)!=1)return 0;
    if(!prelevel){size_t count,j;uint32_t *backdrop,*hud_frame,*pixels;
        SsaSpriteOam saved=a->game->oam,weapon_oam=saved,saved_layers[6];
        SsaSpriteDispatch sprites=a->game->sprites;SsaWeaponObject objects[SSA_SPRITE_CAPACITY];int ok;
        /* Capture the current renderer before switching presentation owners.
         * Keep every world pixel, including the PC renderer's full resolution. */
        if(!render(a))return 0;
        a->net_width=(unsigned)a->bitmap.bmiHeader.biWidth;a->net_height=(unsigned)-a->bitmap.bmiHeader.biHeight;
        if(!a->net_width || !a->net_height || (uint64_t)a->net_width*a->net_height>SIZE_MAX/sizeof(*pixels))return 0;
        count=(size_t)a->net_width*a->net_height;
        backdrop=malloc(count*sizeof(*backdrop));hud_frame=malloc(count*sizeof(*hud_frame));pixels=malloc(count*sizeof(*pixels));
        if(!backdrop || !hud_frame || !pixels){free(backdrop);free(hud_frame);free(pixels);return 0;}
        memcpy(hud_frame,display_pixels(a),count*sizeof(*hud_frame));
        /* Rebuild weapon-only OAM from private sprite copies. Never change live
         * sprite links, timers, cache or gameplay when capturing the clean frame. */
        for(i=0;i<SSA_SPRITE_CAPACITY;++i){objects[i]=*sprites.objects[i];sprites.objects[i]=&objects[i];}
        sprites.active_count=0;
        for(i=0;i<a->game->sprites.active_count;++i){unsigned slot=a->game->sprites.active[i];
            if(objects[slot].type==2)sprites.active[sprites.active_count++]=slot;}
        memcpy(saved_layers,a->game->original_hud_oam,sizeof saved_layers);
        ok=ssa_native_game_build_oam(a->game,&a->game->oam,&sprites)==1;weapon_oam=a->game->oam;
        if(ok){unsigned suppressed=a->game->hud_suppressed;
            a->game->hud_suppressed=1;a->game->oam=weapon_oam;ok=render(a);
            a->game->oam=saved;a->game->hud_suppressed=suppressed;}
        a->game->oam=saved;memcpy(a->game->original_hud_oam,saved_layers,sizeof saved_layers);
        if(!ok){free(backdrop);free(hud_frame);free(pixels);return 0;}
        memcpy(backdrop,display_pixels(a),count*sizeof(*backdrop));
        a->net_weapon_pieces=weapon_oam.count;a->net_hud_removed=0;
        for(j=0;j<count;++j)a->net_hud_removed+=backdrop[j]!=hud_frame[j];
        free(a->net_backdrop);free(a->net_hud_frame);free(a->net_pixels);
        a->net_backdrop=backdrop;a->net_hud_frame=hud_frame;a->net_pixels=pixels;
    }
    a->net_prelevel=prelevel;if(prelevel)++a->net_briefings;
    release(a);a->net_active=1;a->net_held=a->net_pressed=0;
    a->net_counter=a->game->frame_game.counter;a->net_ticks=0;
    if(prelevel && a->audio.device && !ssa_win32_audio_screen(&a->audio,a->level,7))return 0;
    ++a->transition_frame;return render(a);
}
static int finish_netricsa(App *a)
{
    a->net_active=a->net_pressed=a->net_held=0;
    if(a->net_prelevel){a->net_prelevel=0;return begin_map(a);}
    capture(a);
    ++a->transition_frame;
    free(a->net_backdrop);free(a->net_hud_frame);free(a->net_pixels);a->net_backdrop=a->net_hud_frame=a->net_pixels=NULL;
    return render(a);
}
static int netricsa_tick(App *a,uint32_t input)
{
    unsigned i;
    /* Only the pre-level screen owns music. In-game NETRICSA leaves the
     * level track, its voices and its playback clock running continuously. */
    if(a->net_prelevel && !a->netricsa->closing && (input&SSA_NET_CLOSE) && !a->no_music)
        ssa_win32_audio_fade(&a->audio,5);
    if((a->net_prelevel?ssa_netricsa_step(a->netricsa,input):ssa_netricsa_pc_step(a->netricsa,input))<0)return 0;
    a->net_pressed=0;++a->net_ticks;++a->transition_frame;
    for(i=0;i<a->netricsa->command_count;++i){unsigned c=a->netricsa->commands[i];
        if(c>=0x100)ssa_win32_audio_play(&a->audio,c-0x100);
        else if(a->net_prelevel || c==12 || c==13 || c==16 || c==17)
            ssa_win32_audio_menu_command(&a->audio,c,a->level,!a->no_music);
    }
    return !a->netricsa->done || finish_netricsa(a);
}
static int netricsa_client_pointer(const App *a,int x,int y,int *out_x,int *out_y)
{
    RECT bounds;int ix=x,iy=y;
    if(!GetClientRect(a->window,&bounds))return 0;
    if(a->net_prelevel)return ssa_ui_pointer(bounds.right,bounds.bottom,240,160,x,y,out_x,out_y);
    if(!ssa_ui_pointer(bounds.right,bounds.bottom,(int)a->net_width,(int)a->net_height,x,y,&ix,&iy))return 0;
    return ssa_ui_pointer((int)a->net_width,(int)a->net_height,240,160,ix,iy,out_x,out_y);
}
static void netricsa_pointer(App *a,int x,int y,unsigned click)
{
    int result=ssa_netricsa_pointer(a->netricsa,x,y,click);
    if(result==SSA_NET_POINTER_CLOSE){a->net_pressed|=SSA_NET_CLOSE;return;}
    if(result<0)a->failed=1;
    if(result>0)ssa_win32_audio_menu_command(&a->audio,12,a->level,!a->no_music);
    if(click && result>=0 && a->netricsa->hover &&
       !(a->netricsa->hover==2 && !a->netricsa->has_enemies))ssa_win32_audio_menu_command(&a->audio,13,a->level,!a->no_music);
}
static void netricsa_wheel(App *a,int steps)
{
    unsigned weapon=a->netricsa->weapon,enemy=a->netricsa->enemy;
    if(ssa_netricsa_wheel(a->netricsa,steps)<0){a->failed=1;return;}
    if(a->netricsa->weapon!=weapon || a->netricsa->enemy!=enemy)
        ssa_win32_audio_menu_command(&a->audio,12,a->level,!a->no_music);
}
static int begin_menu(App *a)
{
    if(!a->menu){a->menu=calloc(1,sizeof(*a->menu));if(!a->menu)return 0;}
    else memset(a->menu,0,sizeof(*a->menu));
    if(ssa_menu_init(a->menu,a->rom,a->rom_size,!a->no_music)!=1)return 0;
    a->menu->level_limit=SSA_NATIVE_CAMPAIGN_LEVELS;
    a->menu->language=a->language;a->menu->completed_levels=a->profile.data.completed;
    if(ssa_scores_valid(&a->scores.data))a->menu->scores=a->scores.data;
    else{a->scores.data=a->scores.defaults=a->menu->scores;a->scores.last_rank=18;}
    a->menu->settings=a->settings;
    {SsaWin32AudioStatus status=ssa_win32_audio_status(&a->audio);
        a->menu->soundfont_available=status.soundfont_available;a->menu->midi_available=status.midi_available;}
    a->menu_active=1;a->menu_last_page=65535;
    a->menu_pressed=a->menu_held_keys=a->menu_page_ticks=0;
    a->menu_game_counter=a->game->frame_game.counter;return render(a);
}
static int begin_score_menu(App *a,uint32_t score)
{
    unsigned i;
    release(a);
    if(!begin_menu(a) || ssa_menu_score_begin(a->menu,score)!=1 || ssa_menu_step(a->menu,0)!=1)return 0;
    for(i=0;i<a->menu->command_count;++i)ssa_win32_audio_menu_command(&a->audio,a->menu->commands[i],a->level,!a->no_music);
    ssa_win32_audio_menu_command(&a->audio,8,a->level,!a->no_music);
    return render(a);
}
static int begin_story(App *a,unsigned next_level)
{
    release(a);
    if(!a->story){a->story=calloc(1,sizeof(*a->story));if(!a->story)return 0;}
    else memset(a->story,0,sizeof(*a->story));
    if(ssa_campaign_screen_init(a->story,a->rom,a->rom_size,next_level,a->language,!a->no_music)!=1)return 0;
    a->story_active=1;a->story_pressed=a->story_held=0;
    a->story_game_counter=a->game->frame_game.counter;
    a->map_active=a->menu_active=a->level_revealing=0;
    if(a->audio.device && !ssa_win32_audio_begin_story(&a->audio,next_level)){
        fprintf(stderr,"Story audio start failed (%u); continuing silently.\n",a->audio.last_error);
        ssa_win32_audio_close(&a->audio);(void)ssa_native_game_bind_audio(a->game,NULL);
    }
    return render(a);
}
static int finish_story(App *a)
{
    unsigned ending=a->story->next_level==12;
    a->story_active=a->story_pressed=a->story_held=0;
    if(!ending)return begin_netricsa(a,1);
    return begin_score_menu(a,a->tally_recorded?a->completed_carry.score:a->game->score);
}
