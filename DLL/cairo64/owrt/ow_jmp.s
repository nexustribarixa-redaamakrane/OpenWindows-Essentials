/* ow_jmp.s — x86-64 setjmp/longjmp for the vendored Cairo/pixman.
 * The genuine scan converters (bentley-ottmann, tor, botor, ...) use
 * setjmp/longjmp to unwind a polygon-intersection scan. The CRT provides
 * both, which freestanding builds must not import, so the pair is
 * implemented here against the mingw-w64 __JUMP_BUFFER layout:
 *   jmp_buf == __JUMP_BUFFER { void *Frame; (then 9 saved vals) }
 *   Frame  +0x00, Rbx +0x08, Rsp +0x10, Rbp +0x18, Rsi +0x20,
 *   Rdi    +0x28, R12  +0x30, R13  +0x38, R14  +0x40, R15  +0x48,
 *   Rip    +0x50, Spare+0x58.
 * Pairs register r11 = entry RSP (pointing at the saved RIP) so longjmp
 * can resume by re-pushing that RIP and ret-picking it with RSP restored.
 */

	.text

	.p2align	4
	.globl	_setjmp
_setjmp:
	movq	%rsp, %r11			# r11 = entry rsp
	movq	(%r11), %rax			# rax = saved return address
	movq	$0,   0x00(%rcx)		# Frame
	movq	%rbx, 0x08(%rcx)		# Rbx
	movq	%r11, 0x10(%rcx)		# Rsp
	movq	%rbp, 0x18(%rcx)		# Rbp
	movq	%rsi, 0x20(%rcx)		# Rsi
	movq	%rdi, 0x28(%rcx)		# Rdi
	movq	%r12, 0x30(%rcx)		# R12
	movq	%r13, 0x38(%rcx)		# R13
	movq	%r14, 0x40(%rcx)		# R14
	movq	%r15, 0x48(%rcx)		# R15
	movq	%rax, 0x50(%rcx)		# Rip
	movq	$0,   0x58(%rcx)		# Spare
	xorl	%eax, %eax
	ret

	.p2align	4
	.globl	longjmp
longjmp:
	movl	%edx, %r8d			# r8d = return value
	movq	0x10(%rcx), %r9			# r9  = saved Rsp
	movq	0x50(%rcx), %r10		# r10 = saved Rip
	movq	0x08(%rcx), %rbx
	movq	0x18(%rcx), %rbp
	movq	0x20(%rcx), %rsi
	movq	0x28(%rcx), %rdi
	movq	0x30(%rcx), %r12
	movq	0x38(%rcx), %r13
	movq	0x40(%rcx), %r14
	movq	0x48(%rcx), %r15
	movq	%r10, (%r9)			# make ret-pop the saved RIP
	movq	%r9, %rsp
	movl	%r8d, %eax
	testl	%eax, %eax
	jne	1f
	movl	$1, %eax
1:	ret

	/* import-thunk cell: genuine objects call longjmp via __imp_longjmp */
	.data
	.p2align	3
	.globl	__imp_longjmp
__imp_longjmp:
	.quad	longjmp