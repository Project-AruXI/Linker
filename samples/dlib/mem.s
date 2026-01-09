.glob alloc
.glob free

.text
	alloc:
		ld x1, =TALLOC
		str x0, [x1]
		ret

	free:
		ld x1, =TALLOC
		ld xz, [x1]
		ret


.bss
	TALLOC: .zero #1024
	GALLOC: .zero #1024

.data
	TALLOC_PTR: .word TALLOC
	GALLOC_PTR: .word GALLOC