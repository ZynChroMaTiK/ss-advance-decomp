/* Independent v17 fixture encoder for migration tests; not runtime code. */
static int settings_legacy(const SsaSettings *s,uint8_t *b){
    uint32_t crc=UINT32_MAX;unsigned i,j;
    memcpy(b,"SSA-OPT",7);b[0]|=128;b[5]|=(uint8_t)(s->godrays<<7);b[6]|=(uint8_t)(s->mipmaps<<7);
    b[7]=(uint8_t)(s->vertical_aim|(s->fov_offset<<1)|((s->renderer>>1)<<7));
    b[8]=(uint8_t)(1|(s->water_reflections<<4)|(s->surface_decals<<5)|(s->weapon_bobbing<<6)|(s->aspect_ratio<<7));
    b[9]=(uint8_t)(s->music_style|((s->renderer&1)<<2)|(s->jump_enabled<<3)|(s->ingame_netricsa<<4)|(s->thirdperson_enabled<<5)|(s->display_mode<<6)|(s->alternate_weapons<<7));
    b[10]=(uint8_t)(s->music_volume|(s->automap_enabled<<7));b[11]=(uint8_t)(s->sound_volume|(s->hud_style<<7));
    for(i=0;i<12;++i){crc^=b[i];for(j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}
    crc=~crc;for(i=0;i<4;++i)b[12+i]=(uint8_t)(crc>>(8*i));return 1;
}
