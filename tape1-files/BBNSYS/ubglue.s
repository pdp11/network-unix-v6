	.globl	_Xdhrint0
	.align	2
_Xdhrint0:
	pushr	$0x3f
	pushl	$0
	calls	$1,_dhrint
	popr	$0x3f
	rei

	.globl	_Xdhxint0
	.align	2
_Xdhxint0:
	pushr	$0x3f
	pushl	$0
	calls	$1,_dhxint
	popr	$0x3f
	rei

	.globl	_Xdzrint0
	.align	2
_Xdzrint0:
	pushr	$0x3f
	pushl	$0
	calls	$1,_dzrint
	popr	$0x3f
	rei

	.globl	_Xdzxint0
	.align	2
_Xdzxint0:
	pushr	$0x3f
	movl	$0,r0
	jbr	_dzdma

	.globl	_Xdzrint1
	.align	2
_Xdzrint1:
	pushr	$0x3f
	pushl	$1
	calls	$1,_dzrint
	popr	$0x3f
	rei

	.globl	_Xdzxint1
	.align	2
_Xdzxint1:
	pushr	$0x3f
	movl	$1,r0
	jbr	_dzdma

	.globl	_Xdzrint2
	.align	2
_Xdzrint2:
	pushr	$0x3f
	pushl	$2
	calls	$1,_dzrint
	popr	$0x3f
	rei

	.globl	_Xdzxint2
	.align	2
_Xdzxint2:
	pushr	$0x3f
	movl	$2,r0
	jbr	_dzdma

	.globl	_Xacc_iint0
	.align	2
_Xacc_iint0:
	pushr	$0x3f
	pushl	$0
	calls	$1,_acc_iint
	popr	$0x3f
	rei

	.globl	_Xacc_oint0
	.align	2
_Xacc_oint0:
	pushr	$0x3f
	pushl	$0
	calls	$1,_acc_oint
	popr	$0x3f
	rei

	.globl	_Xacc_iint1
	.align	2
_Xacc_iint1:
	pushr	$0x3f
	pushl	$1
	calls	$1,_acc_iint
	popr	$0x3f
	rei

	.globl	_Xacc_oint1
	.align	2
_Xacc_oint1:
	pushr	$0x3f
	pushl	$1
	calls	$1,_acc_oint
	popr	$0x3f
	rei

