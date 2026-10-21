/* GBA actor_queue.c clips artwork by nearer wall silhouettes, not by the
 * floor/ceiling depth at each texel. Work in the upright billboard plane so
 * the same silhouette survives native pitch, FOV and reflected cameras. */
typedef struct ActorClipWall {
    double left,right,bottom_slope,bottom_offset,top_slope,top_offset;
    unsigned flags; /* bottom=1, top=2, closed=4 */
} ActorClipWall;
typedef struct ActorClipPoint {double x,z;} ActorClipPoint;
typedef struct ActorViewWall {
    double ax,ay,bx,by,cam_side,ad,bd,al,bl,near_ad,near_bd;
    int ff,fc,bf,bc;unsigned flags;
} ActorViewWall;
typedef struct ActorClipScratch {
    ActorClipWall *walls;double *cuts;ActorClipPoint *points;
    ActorViewWall *view;unsigned view_count;size_t capacity;
} ActorClipScratch;
static void actor_clip_free(ActorClipScratch *s)
{free(s->walls);free(s->cuts);free(s->points);free(s->view);memset(s,0,sizeof(*s));}
static int actor_clip_reserve(ActorClipScratch *s,size_t n)
{
    ActorClipScratch next={0};
    if(s->capacity>=n+1)return 1;
    next.capacity=n+1;
    next.walls=malloc(next.capacity*sizeof(*next.walls));
    next.cuts=malloc((2*n+2)*sizeof(*next.cuts));
    next.view=malloc(next.capacity*sizeof(*next.view));
    next.points=malloc(2*(2*n+8)*sizeof(*next.points));
    if(!next.walls || !next.cuts || !next.points || !next.view){actor_clip_free(&next);return -1;}
    actor_clip_free(s);*s=next;return 1;
}
static int actor_cut_compare(const void *a,const void *b)
{double x=*(const double *)a,y=*(const double *)b;return x<y?-1:x>y;}
/* Clip to z >= slope*x+offset (bottom), or <= it (top). */
static unsigned actor_clip_height(const ActorClipPoint *in,unsigned n,ActorClipPoint *out,
    double slope,double offset,int bottom)
{
    unsigned i,count=0;
    for(i=0;i<n;++i){ActorClipPoint a=in[i],b=in[(i+1)%n];
        double da=a.z-slope*a.x-offset,db=b.z-slope*b.x-offset;
        if(!bottom){da=-da;db=-db;}
        if(da>=0)out[count++]=a;
        if((da>=0)!=(db>=0)){double t=da/(da-db);
            out[count++]=(ActorClipPoint){a.x+(b.x-a.x)*t,a.z+(b.z-a.z)*t};}
    }
    return count;
}
