; =============================================================================
; VMArea Phase 1 — Minimal Guest Kernel
; =============================================================================
;
; A minimal 16-bit real-mode kernel for the VMArea hypervisor.
; Runs inside a Windows Hypervisor Platform (WHP) virtual machine.
;
; Behavior:
;   1. Initializes segment registers
;   2. Outputs a boot message via COM1 serial port (I/O port 0x3F8)
;   3. Halts the CPU (triggers WHvRunVpExitReasonX64Halt in the VMM)
;
; Build: nasm -f bin -o guest_kernel.bin kernel.asm
; =============================================================================

bits 16
org 0

_start:
    ; Initialize segment registers to 0 (flat real-mode addressing)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack pointer (conventional location)

    ; Print boot message character by character via COM1
    mov si, boot_message

.print_loop:
    lodsb                    ; Load byte at DS:SI into AL, increment SI
    test al, al              ; Check for null terminator
    jz .halt                 ; If zero, we're done printing

    mov dx, 0x03F8           ; COM1 data register (I/O port)
    out dx, al               ; Write character — triggers VM exit in VMM

    jmp .print_loop          ; Next character

.halt:
    hlt                      ; Halt CPU — VMM intercepts this exit
    jmp .halt                ; Safety loop in case HLT somehow returns

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
boot_message:
    db "Darwin-like environment booted successfully.", 13, 10, 0

; Pad binary to 512 bytes (ensures minimum image size)
times 512 - ($ - $$) db 0
