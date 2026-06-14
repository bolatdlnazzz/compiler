default rel
section .data
section .text
global main
extern astra_print_i32
extern astra_print_i64
extern astra_print_u32
extern astra_print_u64
extern astra_print_f32
extern astra_print_f64
extern astra_print_bool
extern astra_print_char
extern astra_print_string
extern astra_input_string
extern astra_exit
extern astra_panic
extern astra_assert
extern astra_string_concat
extern astra_string_eq
extern astra_string_ne
extern astra_string_len
extern astra_rt_div_zero
extern astra_rt_oob
astra_bad__int32:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, rdi
    lea rdi, [rbp - 4]
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    movsxd rax, eax
    mov rbx, rax
    pop rax
    cmp rax, rbx
    setg al
    movzx rax, al
    cmp rax, 0
    je .Lelse_1
    mov rax, 1
    movsxd rax, eax
    jmp .Lreturn_0
    jmp .Lendif_2
.Lelse_1:
.Lendif_2:
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

main:
    push rbp
    mov rbp, rsp
    mov rax, 1
    movsxd rax, eax
    push rax
    pop rdi
    call astra_bad__int32
    jmp .Lreturn_3
.Lreturn_3:
    mov rsp, rbp
    pop rbp
    ret

