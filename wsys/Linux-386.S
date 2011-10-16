	.file	"Linux-386.S"
	.text

        .type   _tas,@function
        .globl  _tas
_tas:
	movl	$1, %eax
	movl	4(%esp), %ecx
	xchgl	%eax, 0(%ecx)
	ret
