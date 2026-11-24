	.text
	# Input: seconds. Output: remaining seconds on the previous alarm.
	.globl __crunch_alarm
	.type __crunch_alarm, @function
__crunch_alarm:
	mov $37, %eax
	syscall
	ret
	.size __crunch_alarm, .-__crunch_alarm
	.section .note.GNU-stack,"",@progbits
