[bits 32]
section .text
global switch_to
switch_to:
    ; ret addr
    push esi
    push edi
    push ebx
    push ebp

    mov eax,[esp+20] ; get para in stack; e.g. cur: [esp+20]
    mov [eax],esp    ;  //[esp]: TASK_STRUCT self_kstack

    mov eax,[esp+24]    ; get para in stack; e.g. next 
    mov esp, [eax]      ; recover esp to next's esp

    pop ebp
    pop ebx
    pop edi
    pop esi
    ret                 ;return to ret addr