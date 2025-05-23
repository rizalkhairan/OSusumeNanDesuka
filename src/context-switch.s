global process_context_switch

; Load struct Context (CPU GP-register) then jump
; Function Signature: void process_context_switch(struct Context ctx);
process_context_switch:
    ; Using iret (return instruction for interrupt) technique for privilege change
    lea  ecx, [esp + 0x04] ; Save the base address for struct Context ctx


    ; Using iret (return instruction for interrupt) technique for privilege change
    ; Stack values will be loaded into these register:
    ; [esp] -> eip, [esp+4] -> cs, [esp+8] -> eflags, [] -> user esp, [] -> user ss
    push 0x23 ; Stack segment selector (GDT_USER_DATA_SELECTOR), user privilege
    mov  eax, [ecx + 0xc] 
    ; mov  eax, esp
    ; add  eax, 0x400000 - 4
    push eax ; User space stack pointer (esp)
    mov  eax, [ecx + 0x34]
    push eax    ; eflags register state, when jump inside user program
    mov  eax, 0x18 | 0x3
    push eax ; Code segment selector (GDT_USER_CODE_SELECTOR), user privilege
    mov  eax, [ecx + 0x30]
    push eax ; eip register to jump back

    ; Segment registers
    mov ds, [ecx + 0x2c] 
    mov es, [ecx + 0x28]
    mov fs, [ecx + 0x24]
    mov gs, [ecx + 0x20]

    mov ebp, [ecx + 0x8]

    ; General registers
    mov edx, [ecx + 0x14]
    mov ebx, [ecx + 0x10]

    ; Index registers
    mov esi, [ecx + 0x4] 
    mov edi, [ecx + 0]

    ; cr3
    ; mov eax, [ecx + 0x38]
    ; push eax
    ; call 0xc01047dc
;     cmp eax, 0xC0000000
;     jg cre
;     sub eax, 0xC0000000

; cre:
;     mov cr3, eax

    mov eax, [ecx + 0x14] ; eax, pushed after cr3 because we used it to hold page_directory_virtual_address
    mov ecx, [ecx + 0x18] ; ecx, goes last because we used it to hold ctx


    iret