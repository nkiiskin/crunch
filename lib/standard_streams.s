	.data
	.balign 8
.Lstdin_stream:
	.long 0, 0, 0, 0
.Lstdout_stream:
	.long 1, 1, 0, 0
.Lstderr_stream:
	.long 2, 1, 0, 0

	.globl _crunch_stdin
	.ifndef CRUNCH_SHARED
	.hidden _crunch_stdin
	.endif
	.type _crunch_stdin, @object
	.size _crunch_stdin, 8
_crunch_stdin:
	.quad .Lstdin_stream
	.globl _crunch_stdout
	.ifndef CRUNCH_SHARED
	.hidden _crunch_stdout
	.endif
	.type _crunch_stdout, @object
	.size _crunch_stdout, 8
_crunch_stdout:
	.quad .Lstdout_stream
	.globl _crunch_stderr
	.ifndef CRUNCH_SHARED
	.hidden _crunch_stderr
	.endif
	.type _crunch_stderr, @object
	.size _crunch_stderr, 8
_crunch_stderr:
	.quad .Lstderr_stream
	.section .note.GNU-stack,"",@progbits
