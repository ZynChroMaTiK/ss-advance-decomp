/* Test-only input scripts. Called only by the explicit diagnostic modes. */
static int netricsa_pointer_check(App *a,int x,int y,int wheel)
{
    RECT bounds;int left,top,width,height,px,py;LPARAM point;
    int rw=a->net_prelevel?240:(int)a->net_width,rh=a->net_prelevel?160:(int)a->net_height;
    if(!GetClientRect(a->window,&bounds) || !ssa_ui_viewport(rw,rh,240,160,&left,&top,&width,&height))return 0;
    px=left+(x*width+width/2)/240;py=top+(y*height+height/2)/160;
    if(!ssa_ui_viewport(bounds.right,bounds.bottom,rw,rh,&left,&top,&width,&height))return 0;
    point=MAKELPARAM(left+(px*width+width/2)/rw,top+(py*height+height/2)/rh);
    SendMessageW(a->window,WM_MOUSEMOVE,0,point);
    SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,point);
    SendMessageW(a->window,WM_LBUTTONUP,0,point);
    if(wheel)SendMessageW(a->window,WM_MOUSEWHEEL,MAKEWPARAM(0,wheel*WHEEL_DELTA),0);
    return !a->failed;
}
static void diagnostics_audio(App *a,unsigned frames)
{
    if(a->audio_test && frames>=20 && !a->audio_test_sound){
        if(a->game->audio_ops.play)a->game->audio_ops.play(a->game->audio_ops.context,0x14);
        if(a->audio.device){uint64_t before;unsigned muted;SsaAudioPlayer music;
            EnterCriticalSection(&a->audio.mutex);muted=a->audio.mixer.muted;music=a->audio.mixer.players[0];
            a->audio_test_level_starts=a->no_music?!music.active:music.active && music.song==0x73 && a->audio.mixer.music_command==2;
            a->game->audio_ops.play(a->game->audio_ops.context,13);
            a->audio_test_footsteps=!memcmp(&music,&a->audio.mixer.players[0],sizeof(music)) &&
                audio_test_latest(&a->audio.mixer,13)->active &&
                audio_test_latest(&a->audio.mixer,13)->song==13;
            SendMessageW(a->window,WM_KEYDOWN,'M',0);
            a->audio_test_m_key=a->audio.mixer.muted==muted;
            SendMessageW(a->window,WM_KEYDOWN,'M',0);
            a->audio_test_m_key&=a->audio.mixer.muted==muted;
            before=a->audio.recycled;LeaveCriticalSection(&a->audio.mutex);
            /* Deliberately stall rendering beyond the50ms audio queue.
             * The audio worker must continue delivering device buffers. */
            Sleep(160);
            EnterCriticalSection(&a->audio.mutex);a->audio_test_worker=a->audio.recycled>before;
            LeaveCriticalSection(&a->audio.mutex);
        }
        a->audio_test_sound=1;
    }
    if(a->audio_test && a->audio.device && frames>=30 && !a->audio_test_fade){
        EnterCriticalSection(&a->audio.mutex);
        if(ssa_audio_fade(&a->audio.mixer,0)==1)a->audio_test_fade=1;
        LeaveCriticalSection(&a->audio.mutex);
    }
    if(a->audio_test && a->audio.device && frames>=40 && a->audio_test_fade==1){
        EnterCriticalSection(&a->audio.mutex);
        if(a->audio.mixer.master_gain<1 && a->audio.mixer.music.flags&4)a->audio_test_fade=2;
        LeaveCriticalSection(&a->audio.mutex);
    }
    if(a->audio_test && frames>=40 && a->audio_test_footsteps==1 && a->audio.device){SsaAudioPlayer music;
        EnterCriticalSection(&a->audio.mutex);music=a->audio.mixer.players[0];
        a->game->audio_ops.play(a->game->audio_ops.context,14);
        if(!memcmp(&music,&a->audio.mixer.players[0],sizeof(music)) &&
           audio_test_latest(&a->audio.mixer,14)->active &&
           audio_test_latest(&a->audio.mixer,14)->song==14)a->audio_test_footsteps=2;
        LeaveCriticalSection(&a->audio.mutex);
    }
    if(a->audio_test && frames>=80 && !a->audio_test_spatial && a->audio.device){SsaAudioPlayer music;SsaAudioPoint point;
        EnterCriticalSection(&a->audio.mutex);music=a->audio.mixer.players[0];point=a->audio.mixer.listener.position;
        /* The level-clear cue ran before restart. A sticky failure here
         * must not be hidden by the later explicit level-music callback. */
        if(a->audio_test_music!=1 || (a->no_music?music.active:!music.active || music.song!=0x73 || a->audio.mixer.music_command!=2))a->audio_test_music=4;
        else if(a->audio_test_level_starts==1 && a->audio.mixer.master_gain==1 && !(a->audio.mixer.music.flags&12))a->audio_test_level_starts=2;
        point.x+=a->audio.mixer.listener.right_x*256;point.y+=a->audio.mixer.listener.right_y*256;
        a->game->audio_ops.play_at(a->game->audio_ops.context,0x14,&point);
        if(a->audio.mixer.listener_ready && audio_test_latest(&a->audio.mixer,0x14)->positioned &&
           audio_test_latest(&a->audio.mixer,0x14)->target_right>audio_test_latest(&a->audio.mixer,0x14)->target_left*3 &&
           !memcmp(&music,&a->audio.mixer.players[0],sizeof(music)))a->audio_test_spatial=1;
        LeaveCriticalSection(&a->audio.mutex);
    }
    if(a->audio_test && frames>=90 && a->audio_test_spatial==1 && a->audio.device){SsaAudioPoint point;uint64_t notes;
        EnterCriticalSection(&a->audio.mutex);notes=a->audio.mixer.notes;point=a->audio.mixer.listener.position;
        point.x-=a->audio.mixer.listener.right_x*256;point.y-=a->audio.mixer.listener.right_y*256;
        a->game->audio_ops.move(a->game->audio_ops.context,0x14,&point);
        if(a->audio.mixer.notes==notes && audio_test_latest(&a->audio.mixer,0x14)->target_left>audio_test_latest(&a->audio.mixer,0x14)->target_right*3)
            a->audio_test_spatial=2;
        LeaveCriticalSection(&a->audio.mutex);
    }
    if(a->audio_test && a->audio.device && a->audio_test_music<3){
        static const unsigned at[]={45,100,110},command[]={9,10,2},song[]={0x78,0x81,0x73};
        unsigned step=a->audio_test_music;
        if(frames>=at[step]){SsaAudioPlayer *player;
            EnterCriticalSection(&a->audio.mutex);
            if(!a->audio_test_music_queued){
                a->game->audio_ops.music(a->game->audio_ops.context,command[step],a->level);a->audio_test_music_queued=1;
            }
            player=&a->audio.mixer.players[0];
            if((a->no_music?!player->active:player->active && player->song==song[step]) && !(a->audio.mixer.music.flags&8)){
                ++a->audio_test_music;a->audio_test_music_queued=0;
            }
            LeaveCriticalSection(&a->audio.mutex);
        }
    }
}
static int diagnostics_menu_input(App *a)
{
    if(a->startup_test){unsigned p=a->menu->page,t=a->menu_page_ticks;
        unsigned wanted_language=a->menu_test?(a->menu_test-1)/3:a->password_test?(a->password_test-1)%3:0;
        unsigned wanted_difficulty=a->menu_test?(a->menu_test-1)%3:1;
        if(p==SSA_MENU_MAIN && (a->menu_test || (a->password_test && wanted_language)) &&
                !a->menu_options_test && (t==2 || t==4))
            SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
        else if(p==SSA_MENU_OPTIONS && (a->menu_test || a->password_test)){
            if(a->menu_credits_test==2){
                if(t==2){SendMessageW(a->window,WM_KEYDOWN,VK_ESCAPE,0);a->menu_options_test=1;}
            }else if(t==6 || t==8)SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
            else if((t==10 && wanted_language>=1) || (t==12 && wanted_language==2))
                SendMessageW(a->window,WM_KEYDOWN,VK_RIGHT,0);
            else if(t==14){
                if(a->password_test){SendMessageW(a->window,WM_KEYDOWN,VK_ESCAPE,0);a->menu_options_test=1;}
                else SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
            }else if(a->menu_test && t==16)SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
            else if(a->menu_test && t==20)SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
        }
        else if(p==SSA_MENU_CREDITS && a->menu_test){
            a->menu_credits_test=1;
            if(t==0){SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);SendMessageW(a->window,WM_KEYUP,VK_UP,0);}
            else if(t==2)SendMessageW(a->window,WM_KEYDOWN,VK_UP,0);
            else if(t==4)SendMessageW(a->window,WM_KEYDOWN,'W',0);
            else if(t==5)SendMessageW(a->window,WM_KEYUP,VK_UP,0);
            else if(t==6){SendMessageW(a->window,WM_KEYUP,'W',0);SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);}
            else if(t==8)SendMessageW(a->window,WM_KEYDOWN,'S',0);
            else if(t==9)SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);
            else if(t==10)SendMessageW(a->window,WM_KILLFOCUS,0,0);
            else if(t==70)SendMessageW(a->window,WM_KEYDOWN,VK_ESCAPE,0);
        }
        else if(p==SSA_MENU_SINGLE && a->password_test && (t==2 || t==4))
            SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
        else if(p==SSA_MENU_SINGLE && a->level_select_test && t==2)
            SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
        else if(p==SSA_MENU_LEVEL_SELECT && a->level_select_test && t>=2)
            SendMessageW(a->window,WM_KEYDOWN,a->menu->selection==a->level_select_test-1?VK_RETURN:VK_DOWN,0);
        else if(p==SSA_MENU_MAIN && a->menu_test && t==8){
            RECT bounds;int ox,oy,ow,oh;unsigned selection=a->menu_options_test?0:2;
            unsigned before,valid;uint64_t starts;SsaAudioPlayer music;
            int x=120+(int)a->menu->offset,y=82+(int)selection*16;
            if(!a->audio.synchronized || !GetClientRect(a->window,&bounds) ||
               !ssa_ui_viewport(bounds.right,bounds.bottom,240,160,&ox,&oy,&ow,&oh))return 0;
            /* Keep the mixer stable while checking real pointer
             * messages. Critical sections permit the cue callback
             * to enter recursively on this same thread. */
            EnterCriticalSection(&a->audio.mutex);starts=a->audio.mixer.starts;music=a->audio.mixer.players[0];
            /* Blank canvas clicks must not submit or make sound. */
            SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(ox+ow/240,oy+oh/160));
            valid=!a->menu_pressed && a->audio.mixer.starts==starts;
            /* Hover another visible label, then the intended row. */
            before=a->menu->selection;
            SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+x*ow/240,oy+90*oh/160));
            starts+=before!=0;valid&=!a->menu->selection && a->audio.mixer.starts==starts;
            before=a->menu->selection;
            SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+x*ow/240,oy+y*oh/160));
            starts+=before!=selection;valid&=a->menu->selection==selection && a->audio.mixer.starts==starts;
            /* Moving within the same label, leaving into blank
             * space and returning, and clicking it stay quiet. */
            SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+(x+1)*ow/240,oy+y*oh/160));
            SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+ow/240,oy+oh/160));
            SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+x*ow/240,oy+y*oh/160));
            SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(ox+x*ow/240,oy+y*oh/160));
            valid&=a->menu_pressed==SSA_MENU_CONFIRM && a->menu->selection==selection &&
                a->audio.mixer.starts==starts && !memcmp(&music,&a->audio.mixer.players[0],sizeof music);
            LeaveCriticalSection(&a->audio.mutex);
            if(!valid)return 0;
            ++a->menu_mouse_checks;
        }
        else if(p==SSA_MENU_PASSWORD && a->password_test && !a->menu->leaving && a->menu->draw_page==SSA_MENU_PASSWORD){
            unsigned index=((a->password_test-1)/PASSWORD_TEST_LEVELS)*12+(a->password_test-1)%PASSWORD_TEST_LEVELS+1;
            const char *target=ssa_menu_password(a->menu,index);
            size_t length;
            if(!target)return 0;
            /* Release menu navigation before original editor hold-repeat. */
            SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);SendMessageW(a->window,WM_KEYUP,VK_UP,0);
            {unsigned j;
                length=strlen(target);while(length && target[length-1]=='_')--length;
                SendMessageW(a->window,WM_CHAR,'x',0);SendMessageW(a->window,WM_CHAR,8,0);
                a->password_test_edges+=2;
                for(j=0;j<length;++j){unsigned c=(unsigned char)target[j];
                    SendMessageW(a->window,WM_KEYDOWN,c,0);
                    if(a->menu_pressed)return 0;
                    SendMessageW(a->window,WM_CHAR,c>='A' && c<='Z'?c+32:c,0);
                    SendMessageW(a->window,WM_KEYUP,c,0);++a->password_test_edges;
                }
                if(strlen(a->menu->password)!=length || strncmp(a->menu->password,target,length))return 0;
            }
            if(a->original_renderer){SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);SendMessageW(a->window,WM_KEYUP,VK_RETURN,0);}
            else{RECT bounds;int ox,oy,ow,oh;
                if(!GetClientRect(a->window,&bounds) || !ssa_ui_viewport(bounds.right,bounds.bottom,240,160,&ox,&oy,&ow,&oh))return 0;
                SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(ox+120*ow/240,oy+120*oh/160));
                SendMessageW(a->window,WM_LBUTTONUP,0,MAKELPARAM(ox+120*ow/240,oy+120*oh/160));
            }
            ++a->password_test_edges;
        }
        else if((p==SSA_MENU_MAIN || p==SSA_MENU_SINGLE) && t==8)
            SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
        else if(p==SSA_MENU_TITLE && a->menu->counter==96)SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
        else if(p==SSA_MENU_DIFFICULTY && ((wanted_difficulty>=1 && t==4) || (wanted_difficulty==2 && t==6)))
            SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
        else if(p==SSA_MENU_DIFFICULTY && t==10)SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
    }
    return 1;
}
static int diagnostics_story_input(App *a)
{
    if(a->startup_test){
        unsigned t=a->story->frame;
        if(t==80)SendMessageW(a->window,WM_KEYDOWN,VK_UP,0);
        if(t==100)SendMessageW(a->window,WM_KEYUP,VK_UP,0);
        if(t==110)SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
        if(t==130)SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);
        if(t==145)SendMessageW(a->window,WM_KEYDOWN,VK_RIGHT,0);
        if(t==150)SendMessageW(a->window,WM_KEYUP,VK_RIGHT,0);
        if(t==160)SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,0);
        if(a->audio.device && !a->story->story.mode){
            unsigned song=0;int valid;
            EnterCriticalSection(&a->audio.mutex);
            valid=a->no_music?!a->audio.mixer.players[0].active:
                ssa_audio_music_select(a->rom,a->rom_size,11,a->story->next_level,1,&song)==1 &&
                a->audio.mixer.music_command==11 && a->audio.mixer.players[0].song==song;
            LeaveCriticalSection(&a->audio.mutex);
            if(!valid)return 0;++a->story_music_checks;
        }
    }
    return 1;
}
