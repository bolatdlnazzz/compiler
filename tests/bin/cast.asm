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
main:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    lea rdi, [rbp - 4]
    push rdi
    mov rax, 65
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rdi, [rbp - 16]
    push rdi
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    pop rdi
    mov qword [rdi], rax
    lea rdi, [rbp - 24]
    push rdi
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    cvtsi2sd xmm0, rax
    pop rdi
    movsd [rdi], xmm0
    lea rax, [rbp - 16]
    mov rax, qword [rax]
    mov rdi, rax
    call astra_print_i64
    xor rax, rax
    lea rax, [rbp - 24]
    movsd xmm0, [rax]
    call astra_print_f64
    xor rax, rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

