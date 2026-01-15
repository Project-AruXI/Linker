.extern read % from std.adlib


.extern file1Fxn

.text
.glob main
main:
	mv x0, #0x1
	call file1Fxn
	call read
	ub _deinit

_deinit:
	mv a1, #0x0
	mv x0, #0x2
	syscall