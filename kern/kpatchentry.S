.global __fentry__
.global _mcount
_mcount:
__fentry__:
	ret
	mov	(%esp), %eax
	sub	$5, %eax
	cmpb	$0xe8, (%eax)
	jne	return
	movb	$0x90, (%eax)
	add	$1, %eax
	movl	$0x90909090, (%eax)
return:
	ret
