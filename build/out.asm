default rel
section .data
section .text
global main
extern astra_print_i32
extern astra_print_i64
extern astra_print_bool
extern astra_print_string
extern astra_input_string
extern astra_exit
extern astra_panic
extern astra_string_len
extern astra_rt_div_zero
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, 42
    mov [rbp-8], rax
    mov rax, [rbp-8]
    mov [rbp-16], rax
    mov rax, [rbp-16]
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

