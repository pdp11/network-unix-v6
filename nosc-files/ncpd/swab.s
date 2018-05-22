/ c library  --  swab
/ <swabbed word> = swab( <word> )
.globl _swab

_swab:
        mov     2(sp),r0
        swab    r0
        rts     pc
