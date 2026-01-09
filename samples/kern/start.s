.text
	__onRun:
	nop
	hlt

	_userStart:
	nop
	ld x10, PS
	ld x0, [sp]
	ubr x0

.data
	PS:
		.word 0x00010100