; =============================================================================
; VanillaOS Boot Entry
; Loaded by GRUB2 in 32-bit protected mode via Multiboot2
; Sets up identity-mapped page tables and enters 64-bit long mode
; Passes multiboot2 info pointer to kernel_main as first argument (rdi)
; =============================================================================

[bits 32]
[extern kernel_main]

; =============================================================================
; Multiboot2 Header (MUST be in first 32KB of kernel image)
; =============================================================================
MB2_MAGIC    equ 0xE85250D6
MB2_ARCH     equ 0                              ; i386 protected mode
MB2_HDRLEN   equ (hdr_end - hdr_start)
MB2_CKSUM    equ (0x100000000 - (MB2_MAGIC + MB2_ARCH + MB2_HDRLEN))

section .multiboot2
align 8
hdr_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_HDRLEN
    dd MB2_CKSUM

    ; --- Framebuffer request tag (type 5) ------------------------------------
    ; Asks GRUB to set a linear framebuffer. GRUB reports the actual address
    ; in the multiboot2 info structure (tag type 8).
    align 8
    dw 5            ; type: framebuffer
    dw 0            ; flags: required (fail if not supported)
    dd 20           ; size: 20 bytes
    dd 1024         ; preferred width
    dd 768          ; preferred height
    dd 32           ; preferred bpp

    ; --- End tag -------------------------------------------------------------
    align 8
    dw 0
    dw 0
    dd 8
hdr_end:

; =============================================================================
; Kernel stack (64 KB, placed in BSS)
; =============================================================================
section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

; =============================================================================
; Data: multiboot2 info pointer saved here for 64-bit access
; =============================================================================
section .data
align 8
multiboot_ptr: dq 0

; =============================================================================
; 64-bit GDT (minimal: null / code64 / data64)
; =============================================================================
section .data
align 8
gdt64:
    dq 0x0000000000000000       ; 0x00 null
    dq 0x00AF9A000000FFFF       ; 0x08 code64: L=1, P=1, DPL=0, exec/read
    dq 0x00AF92000000FFFF       ; 0x10 data64: P=1, DPL=0, read/write

gdt64_ptr:
    dw ($ - gdt64 - 1)         ; limit
    dq gdt64                    ; base

; =============================================================================
; Entry point (GRUB jumps here, EAX=0x36D76289, EBX=mb2_info_ptr)
; =============================================================================
section .text
global _start
_start:
    cli
    mov esp, stack_top          ; set up 32-bit stack

    ; Save multiboot2 info pointer before we need it in 64-bit
    mov [multiboot_ptr], ebx

    ; --- Enable A20 line (port 0x92 method) ----------------------------------
    in  al, 0x92
    or  al, 0x02
    and al, 0xFE
    out 0x92, al

    ; --- Set up identity-mapped page tables (0-4GB, 2MB huge pages) ----------
    ; Layout: PML4@0x1000, PDPT@0x2000, PD0-3@0x3000-0x6000
    ; Zero all 6 pages first
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 0x1800             ; 6144 DWORDs = 24576 bytes
    rep stosd

    ; PML4[0] -> PDPT@0x2000
    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0

    ; PDPT entries (four 1GB slots)
    mov dword [0x2000], 0x3003  ; 0-1 GB  -> PD0@0x3000
    mov dword [0x2008], 0x4003  ; 1-2 GB  -> PD1@0x4000
    mov dword [0x2010], 0x5003  ; 2-3 GB  -> PD2@0x5000
    mov dword [0x2018], 0x6003  ; 3-4 GB  -> PD3@0x6000

    ; Fill all four PDs with 2MB huge-page entries (PS=1, W=1, P=1 -> 0x83)
    mov edi, 0x3000
    mov eax, 0x00000083
    mov ecx, 2048               ; 4 PDs x 512 entries
.fill_pd:
    mov dword [edi],   eax
    mov dword [edi+4], 0
    add eax, 0x200000
    add edi, 8
    loop .fill_pd

    ; --- Enable PAE (required for long mode) ---------------------------------
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    ; --- Load PML4 -----------------------------------------------------------
    mov eax, 0x1000
    mov cr3, eax

    ; --- Enable long mode in EFER MSR ----------------------------------------
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)           ; LME = Long Mode Enable
    wrmsr

    ; --- Enable paging (transitions CPU into compatibility mode) -------------
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax

    ; --- Load 64-bit GDT and jump to 64-bit code segment --------------------
    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_entry

; =============================================================================
; 64-bit long mode entry
; =============================================================================
[bits 64]
long_mode_entry:
    ; Reload all data segment registers with 64-bit data selector
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 64-bit stack
    mov rsp, stack_top

    ; Pass multiboot2 info pointer as first argument (System V AMD64 ABI: rdi)
    mov rdi, [multiboot_ptr]

    ; Clear remaining argument registers
    xor rsi, rsi
    xor rdx, rdx

    ; Jump to C kernel
    call kernel_main

    ; Should never return
.halt:
    cli
    hlt
    jmp .halt
