.text
	_handle0:
	nop
	eret

	_handle1:
	ub endHandle

	_endHandle:
	eret

.evt
	nop
	nop

	.byte 0x00
	.word _handle0
	.byte 0x01
	.word _handle1