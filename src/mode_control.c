#include "ssa/mode_control.h"

int ssa_mode_control_step(SsaModeControl *m, const SsaModeControlOps *ops)
{
    unsigned i;
    int result = -1;
    if (!m || m->updating || !m->flags || !m->input_state || !m->menu_timer ||
        !m->player_count || !m->selection || !ops || !ops->event ||
        *m->player_count > 4) return -1;
    m->updating = 1;
#define EVENT(kind,a,b,c,d) do { if (ops->event(ops->context,m,kind,a,b,c,d)<0) goto done; } while (0)
#define CMD(a) EVENT(SSA_MODE_COMMAND,a,0,0,0)
#define SPR(a) EVENT(SSA_MODE_CREATE_SPRITE,a,0,0x38,0)
    for (i = 0; i < *m->player_count; ++i) {
        if (i >= 4 || !m->inputs[i]) goto done;
        if (*m->flags & 1u) {
            *m->menu_timer = 100;
            if (*m->inputs[i] & UINT32_C(0x90000)) {
                switch (*m->selection) {
                case 0:
                    *m->menu_timer = 0;
                    CMD(5);
                    *m->flags = (uint16_t)((*m->flags | 0x400u) & ~0x41u);
                    break;
                case 1:
                    *m->selection = 3;
                    CMD(12);
                    SPR(0xB2);
                    break;
                case 2:
                    *m->menu_timer = 0;
                    *m->input_state = 5;
                    CMD(5); CMD(6); CMD(13);
                    *m->flags = (uint16_t)((*m->flags | 0x400u) & ~0x41u);
                    break;
                case 3:
                    *m->selection = 0;
                    SPR(0xB1);
                    CMD(13);
                    break;
                default:
                    break;
                }
                if (*m->flags & 0x100u) {
                    *m->input_state = 5;
                    CMD(5); CMD(6); CMD(13);
                }
            }
            if (!(*m->flags & 0x100u)) {
                if (*m->inputs[i] & UINT32_C(0x400000)) {
                    if (*m->selection == 1 || *m->selection == 3)
                        --*m->selection;
                    CMD(12);
                }
                if (*m->inputs[i] & UINT32_C(0x800000)) {
                    if (*m->selection == 0 || *m->selection == 2)
                        ++*m->selection;
                    CMD(12);
                }
            }
        } else if (*m->inputs[i] & UINT32_C(0x80000)) {
            *m->flags |= 1u;
            *m->selection = 0;
            SPR(0xB1);
            CMD(4); CMD(13);
        }
        if (!(*m->flags & 0x800u) && (*m->flags & 0x100u)) {
            *m->flags |= 0x801u;
            SPR(0xB3);
            CMD(4); CMD(13);
        }
        if (*m->player_count > 4) goto done;
    }
    result = 0;
done:
    m->updating = 0;
    return result;
#undef SPR
#undef CMD
#undef EVENT
}
