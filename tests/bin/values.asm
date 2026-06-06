default rel
section .data
LC0: db "test", 0
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
    lea rdi, [rbp - 8]
    push rdi
    lea rax, [rel LC0]
    pop rdi
    mov qword [rdi], rax
    lea rdi, [rbp - 9]
    push rdi
    mov rax, 65
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rdi, [rbp - 10]
    push rdi
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 8]
    mov rax, qword [rax]
    mov rdi, rax
    call astra_print_string
    xor rax, rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    mov rdi, rax
    call astra_print_char
    xor rax, rax
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
    mov rdi, rax
    call astra_print_bool
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

