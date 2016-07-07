    .text
    .align
    .global check_frame_transparency

// Simple algorithm to verify if frame is transparent.
// It scans entire lines and stops when it finds a line
// with non-zero pixel values.
// It starts scanning lines from the center row making
// its way up to the bottom and top of the frame.
// It starts scanning from the center based on the idea that
// non-transparent frames would have its content closer to the
// center more often than not.

// r0: start of frame
// r1: width (must be greater than 32 and multiple of 8)
// r2: height
// r3: stride
check_frame_transparency:
    push {r4-r11, lr}       // stack ARM regs
    vpush {d8-d15}
    mov r4, r2, lsr #1      // r4 and r5 are row numbers
    mov r5, r4
    mov r6, #0              // row counter
    lsl r1, r1, #2          // width in bytes
    mov r10, #1             // default return value
    mov r9, #0              // utility counter
loop:
    cmp r6, r2
    beq end                 // exit after scanning all rows
    add r9, r9, #1
    tst r9, #1              // choose row to scan
    beq loop_r4
    b loop_r5

loop_r4:
    cmp r4, #0
    beq loop                // finished scanning top half
    sub r4, r4, #1
    mla r7, r3, r4, r0      // r7 = base_addr + width*row
    b scan

loop_r5:
    cmp r5, r2
    beq loop                // finished scanning bottom half
    mla r7, r3, r5, r0
    add r5, r5, #1
    b scan

scan:
    add r6, r6, #1      // increase row counter
    vmov.u32 q2, #0
    mov r8, r1          // r8 = width
    vmov.u32 d8, #0     // for residues
scan_1:
    vld1.32 {q0, q1}, [r7]! // load 32 bytes at once
    vorr.u32 q2, q2, q0
    vorr.u32 q2, q2, q1     // q2 > 0 when non-zero value exists
    sub r8, r8, #32
    cmp r8, #32
    bge scan_1              // scan the entire line
    cmp r8, #0
    beq finish_line
scan_residue:
    vld1.32 {d7}, [r7]!
    vorr.u32 d8, d8, d7
    subs r8, r8, #8
    bne scan_residue
finish_line:
    vorr.u32 d6, d4, d5     // move result to smaller registers
    vorr.u32 d6, d6, d8     // in case there was residual data
    vmov r11, r12, d6
    orrs r11, r12
    beq loop                // if transparent line, keep scanning
    mov r10, #0
end:
    mov r0, r10
    vpop {d8-d15}
    pop {r4-r11, pc}


