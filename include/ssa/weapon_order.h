#ifndef SSA_WEAPON_ORDER_H
#define SSA_WEAPON_ORDER_H
/* PC selection slots (zero-based) are distinct from the stable ROM IDs.
 * Time Strike, cannon, UMT occupy slots7,8,9 respectively. */
static inline unsigned ssa_weapon_order_id(unsigned slot)
{
    return slot<7?slot+1:slot==7?9:slot==8?10:slot==9?8:0;
}
static inline unsigned ssa_weapon_order_rank(unsigned id)
{
    return id>=1 && id<=7?id-1:id==9?7:id==10?8:id==8?9:10;
}
#endif
