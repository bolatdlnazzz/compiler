default rel
section .data
LC0: db "hello overload", 0
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
astra_show__int32:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, rdi
    lea rdi, [rbp - 4]
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

astra_show__string:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, rdi
    lea rdi, [rbp - 8]
    mov qword [rdi], rax
    lea rax, [rbp - 8]
    mov rax, qword [rax]
    mov rdi, rax
    call astra_print_string
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_1
.Lreturn_1:
    mov rsp, rbp
    pop rbp
    ret

main:
    push rbp
    mov rbp, rsp
    mov rax, 123
    movsxd rax, eax
    push rax
    pop rdi
    call astra_show__int32
    lea rax, [rel LC0]
    push rax
    pop rdi
    call astra_show__string
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_2
.Lreturn_2:
    mov rsp, rbp
    pop rbp
    ret

