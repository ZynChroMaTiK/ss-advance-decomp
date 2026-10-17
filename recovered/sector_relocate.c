typedef struct ClipSector {
    u16 floor, ceiling;
    u8 pad04[16];
} ClipSector;
typedef struct ClipLeaf {
    u16 start, count, sector, unused;
} ClipLeaf;
typedef struct ClipMap {
    u8 pad00[0x24];
    ClipSector *sectors;
    void *nodes;
    ClipLeaf *leaves;
} ClipMap;
typedef struct ClipEntity {
    s32 x,y,z,vx,vy,vz;
    u8 pad18[0x52];
    s16 sector,node,leaf;
} ClipEntity;
typedef struct ClipScratch {
    s32 x,y,z,end_x,end_y,end_z,radius,distance;
    u16 line,leaf,sector;
} ClipScratch;
extern ClipMap *D_03004930;
extern ClipScratch D_03003F10;
extern s32 sub_0800CE50(void);
extern void sub_0800D214(ClipEntity *);
extern void sub_0800D1B4(ClipEntity *);

s32 sub_080053D0(ClipEntity *entity) {
    s32 moved=0;
    ClipSector *sector;
    s32 floor;
    D_03003F10.radius=0x200000;
    D_03003F10.x=entity->x;
    D_03003F10.y=entity->y;
    D_03003F10.z=entity->z;
    D_03003F10.end_x=D_03003F10.x+entity->vx;
    D_03003F10.end_y=D_03003F10.y+entity->vy;
    D_03003F10.end_z=D_03003F10.z+entity->vz;
    D_03003F10.leaf=entity->leaf;
    D_03003F10.sector=D_03004930->leaves[entity->leaf].sector;
    if(sub_0800CE50()) {sub_0800D214(entity);moved=1;}
    entity->x=D_03003F10.end_x;
    entity->y=D_03003F10.end_y;
    entity->z=D_03003F10.end_z;
    entity->leaf=D_03003F10.leaf;
    entity->sector=D_03004930->leaves[entity->leaf].sector;
    sector=&D_03004930->sectors[entity->sector];
    floor=sector->floor<<16;
    if(entity->z<floor)entity->z=floor;
    else if(entity->z>(sector->ceiling<<16))entity->z=sector->ceiling<<16;
    if(moved)sub_0800D1B4(entity);
    return moved;
}
