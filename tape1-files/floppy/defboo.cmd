!
! BOOTSTRAP BY BOOT() SYSCALL
! GO MULTI-USER AFTER CHECKING; BOOT FROM DEFAULT DEVICE
!
SET DEF HEX
SET DEF LONG
SET REL:0
HALT
UNJAM
INIT
LOAD BOOT
D/G B 0		! BOOT PARAMETERS: MULTI USER AFTER CHECK
D/G A 0		! DEV TO BOOT FROM (0=HP, 2=UP, 3=HK)
START 2
