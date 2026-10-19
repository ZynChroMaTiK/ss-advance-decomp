#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"settings line%d: %s\n",__LINE__,#x);exit(1); } } while(0)
int main(void)
{
    SsaSettings base=SSA_SETTINGS_DEFAULT,next,decoded;
    uint8_t bytes[SSA_SETTINGS_BYTES],encoded[SSA_SETTINGS_BYTES];
    CHECK(ssa_settings_valid(&base));CHECK(ssa_settings_equal(&base,&base));
    CHECK(!ssa_settings_valid(NULL));CHECK(!ssa_settings_equal(&base,NULL));
    CHECK(ssa_settings_encode(&base,bytes,sizeof bytes)==1);
#define CHECK_FIELD(name,max) \
    next=base;next.name=base.name?0:(max); \
    CHECK(ssa_settings_valid(&next));CHECK(!ssa_settings_equal(&base,&next)); \
    CHECK(ssa_settings_encode(&next,encoded,sizeof encoded)==1); \
    CHECK(memcmp(bytes,encoded,sizeof bytes)); \
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1); \
    CHECK(ssa_settings_equal(&next,&decoded)); \
    next.name=(max)+1;CHECK(!ssa_settings_valid(&next)); \
    CHECK(ssa_settings_encode(&next,encoded,sizeof encoded)==-1);
    SSA_SETTINGS_FIELDS(CHECK_FIELD)
#undef CHECK_FIELD
    printf("{\"result\":\"pass\",\"checks\":%u}\n",checks);return 0;
}
