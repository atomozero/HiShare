/*  pirate_asm.S — the 8-bit chiptune waveform, synthesized by hand in x86-64
 *  assembly (because a proper demo's music is *naturally* in ASM).
 *
 *  void pirate_square_fill(int16_t *out, uint32_t frames,
 *                          uint32_t *phase, uint32_t phaseInc, int16_t amp);
 *
 *  Classic phase-accumulator square-wave oscillator: a 32-bit phase counter is
 *  advanced by phaseInc each sample (it wraps naturally); the top bit selects
 *  +amp / -amp.  Branchless, integer-only, position-independent (touches memory
 *  only through the caller's pointers), all scratch registers — SysV AMD64 ABI:
 *      rdi = out    esi = frames    rdx = &phase    ecx = phaseInc    r8w = amp
 */
	.intel_syntax noprefix
	.text
	.globl	pirate_square_fill
	.type	pirate_square_fill, @function
pirate_square_fill:
	test	esi, esi            /* frames == 0 ? */
	je	2f
	mov	eax, dword ptr [rdx] /* eax = *phase                        */
	movsx	r8d, r8w            /* r8d = amp (sign-extended to 32 bit) */
1:
	add	eax, ecx            /* phase += phaseInc (wraps at 2^32)   */
	mov	r9d, eax
	sar	r9d, 31             /* r9d = 0 (top bit clear) or -1 (set) */
	mov	r10d, r8d
	xor	r10d, r9d
	sub	r10d, r9d           /* r10d = (amp^mask)-mask = +amp or -amp */
	mov	word ptr [rdi], r10w /* store the 16-bit sample             */
	add	rdi, 2
	dec	esi
	jnz	1b
	mov	dword ptr [rdx], eax /* write the phase back for next call  */
2:
	ret
	.size	pirate_square_fill, .-pirate_square_fill

	/* Non-executable stack (Haiku/GNU convention). */
	.section .note.GNU-stack, "", @progbits
	.att_syntax prefix
