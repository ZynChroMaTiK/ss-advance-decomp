/* FC3C matching candidate; pointers are original GBA addresses here. */
void sub_0800FC3C(void)
{
    u32 *current=(u32 *)0x030055D0,*next;
    int remaining=15;
    do {
        next=current+1;
        *current=(u32)next;
        --remaining;
        current=next;
    } while(remaining>=0);
    *next=0;
}
