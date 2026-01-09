.glob write
.glob read

.text
	write:
		str x0, [x1]
		syscall

	read:
		ld x0, [x1]
		syscall

.data
	BUFFER: .fill #64, #1