extern void sub_08013940(void *);
extern void sub_08013924(void *);
extern void sub_0801385C(void *);
extern void sub_080137DC(void *);

void sub_08013978(void *entity)
{
    switch (*(u8 *)((u8 *)entity + 0x3e)) {
    case 0:
        sub_08013940(entity);
        break;
    case 1:
        sub_08013924(entity);
        break;
    case 2:
        sub_0801385C(entity);
        break;
    }
    sub_080137DC(entity);
}
