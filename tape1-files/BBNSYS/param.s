LL0:
	.data
	.comm	_hz,4
	.comm	_timezone,4
	.comm	_dstflag,4
	.comm	_canonb,256
	.comm	_version,0
	.comm	_lbolt,4
	.comm	_time,4
	.comm	_bootime,4
	.comm	_hand,4
	.comm	_nblkdev,4
	.comm	_nchrdev,4
	.comm	_nswdev,4
	.comm	_mpid,4
	.comm	_runin,1
	.comm	_runout,1
	.comm	_runrun,4
	.comm	_kmapwnt,1
	.comm	_curpri,1
	.comm	_maxmem,4
	.comm	_physmem,4
	.comm	_nswap,4
	.comm	_updlock,4
	.comm	_rablock,4
	.comm	_rootdev,2
	.comm	_dumpdev,2
	.comm	_dumplo,4
	.comm	_swapdev,2
	.comm	_argdev,2
	.comm	_pipedev,2
	.comm	_vmmap,0
	.comm	_umbabeg,4
	.comm	_umbaend,4
	.comm	_noproc,4
	.comm	_panicstr,4
	.comm	_wantin,4
	.comm	_boothowto,4
	.align	2
	.globl	_hz
_hz:
	.long	60
	.align	2
	.globl	_timezone
_timezone:
	.long	300
	.align	2
	.globl	_dstflag
_dstflag:
	.long	1
	.align	2
	.globl	_nproc
_nproc:
	.long	404
	.align	2
	.globl	_ntext
_ntext:
	.long	72
	.align	2
	.globl	_ninode
_ninode:
	.long	500
	.align	2
	.globl	_nfile
_nfile:
	.long	406
	.align	2
	.globl	_ncallout
_ncallout:
	.long	64
	.align	2
	.globl	_nclist
_nclist:
	.long	868
	.align	2
	.globl	_nnetpages
_nnetpages:
	.long	256
	.align	2
	.globl	_nwork
_nwork:
	.long	20
	.align	2
	.globl	_nnetcon
_nnetcon:
	.long	20
	.align	2
	.globl	_nhost
_nhost:
	.long	20
	.comm	_nbuf,4
	.comm	_nswbuf,4
	.comm	_proc,4
	.comm	_procNPROC,4
	.comm	_text,4
	.comm	_textNTEXT,4
	.comm	_inode,4
	.comm	_inodeNINODE,4
	.comm	_file,4
	.comm	_fileNFILE,4
	.comm	_callout,4
	.comm	_cfree,4
	.comm	_buf,4
	.comm	_swbuf,4
	.comm	_swsize,4
	.comm	_swpf,4
	.comm	_buffers,4
	.comm	_cmap,4
	.comm	_ecmap,4
	.comm	_freetab,4
	.comm	_workfree,4
	.comm	_workNWORK,4
	.comm	_contab,4
	.comm	_conNCON,4
	.comm	_host,4
	.comm	_hostNHOST,4
	.comm	_netcb,176
	.comm	_netstat,32
