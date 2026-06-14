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
    sub rsp, 16
    lea rdi, [rbp - 4]
    push rdi
    mov rax, 2
    movsxd rax, eax
    push rax
    mov rax, 3
    movsxd rax, eax
    push rax
    mov rax, 4
    movsxd rax, eax
    mov rbx, rax
    pop rax
    imul rax, rbx
    movsxd rax, eax
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rdi, [rbp - 8]
    push rdi
    mov rax, 2
    movsxd rax, eax
    push rax
    mov rax, 3
    movsxd rax, eax
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    push rax
    mov rax, 4
    movsxd rax, eax
    mov rbx, rax
    pop rax
    imul rax, rbx
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 8]
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

