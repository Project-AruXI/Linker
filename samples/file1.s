.glob file1Fxn

.text
	nop
file1Fxn:
	ld x10, =arr
	ld x1, [x10, #0x1] % Get second element (0x2)
	add x0, x0, x1
	ret

.data
arr:
	.byte #0x1, #0x2, #0x3, #0x4, #0x5