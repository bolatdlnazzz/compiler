default rel
section .data
LC0: db "int32", 0
LC1: db "int32", 0
LC2: db "dop_meta::Point", 0
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
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rdi, [rbp - 4]
    push rdi
    mov rax, 10
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rdi, [rbp - 12]
    push rdi
    push rdi
    mov rax, 3
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    push rdi
    add rdi, 4
    push rdi
    mov rax, 4
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    mov rax, 4
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 8
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rel LC0]
    mov rdi, rax
    call astra_print_string
    xor rax, rax
    lea rax, [rel LC1]
    mov rdi, rax
    call astra_print_string
    xor rax, rax
    lea rax, [rel LC2]
    mov rdi, rax
    call astra_print_string
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

