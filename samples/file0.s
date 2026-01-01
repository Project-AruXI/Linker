.extern file1Fxn

.text
_init:
	mv x0, #0x1
	call file1Fxn
	ub _deinit

_deinit:
	mv a1, #0x0
	mv x0, #0x2
	syscall