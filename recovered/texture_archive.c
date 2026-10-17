typedef struct TextureRecord {u8 name[8];u16 width,height;u32 data;} TextureRecord;
typedef struct TextureHeader {u8 prefix[8];u16 count;u8 pad[22];TextureRecord *records;u8 tail[36];} TextureHeader;
extern u8 *D_0844AEF4[];
extern u8 *D_03004010;
extern volatile u32 D_040000D4[];
extern void *sub_080033D8(u32);
extern void sub_080C5320(const void *,void *,u32);
TextureHeader *sub_0800AFF4(u32 id) {
    u8 *base=D_0844AEF4[id];
    TextureHeader *out=sub_080033D8(72);
    TextureRecord *record;
    u32 i,offset;
    if(D_03004010[16]>1)sub_080C5320(base,out,36);
    else {D_040000D4[0]=(u32)base;D_040000D4[1]=(u32)out;D_040000D4[2]=0x80000024;D_040000D4[2];}
    if(out->count) {
        record=(TextureRecord *)(base+*(u32 *)(base+32));
        out->records=sub_080033D8(out->count*16);
        i=0;
        if(i<out->count) {
        offset=0;
        do {
            if(D_03004010[16]>1)sub_080C5320(record,(u8 *)out->records+offset,8);
            else {D_040000D4[0]=(u32)record;D_040000D4[1]=(u32)out->records+offset;D_040000D4[2]=0x80000008;D_040000D4[2];}
            ((TextureRecord *)(offset+(u8 *)out->records))->data=(u32)base+record->data;
            offset+=16;++i;++record;
        } while(i<out->count);
        }
    }
    else return NULL;
    return out;
}
