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
astra_a3_01_overload_implicit_cast__choose__int64:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, rdi
    lea rdi, [rbp - 8]
    mov qword [rdi], rax
    lea rax, [rbp - 8]
    mov rax, qword [rax]
    push rax
    mov rax, 1000
    movsxd rax, eax
    mov rbx, rax
    pop rax
    add rax, rbx
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

main:
    push rbp
    mov rbp, rsp
    mov rax, 5
    push rax
    pop rdi
    call astra_a3_01_overload_implicit_cast__choose__int64
    mov rdi, rax
    call astra_print_i64
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_1
.Lreturn_1:
    mov rsp, rbp
    pop rbp
    ret

