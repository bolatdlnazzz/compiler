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
    mov rax, 6
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rdi, [rbp - 8]
    push rdi
    mov rax, 3
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    and rax, rbx
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    or rax, rbx
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    xor rax, rbx
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    mov rax, 1
    movsxd rax, eax
    mov rbx, rax
    pop rax
    mov rcx, rbx
    and cl, 31
    shl rax, cl
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    mov rax, 1
    movsxd rax, eax
    mov rbx, rax
    pop rax
    mov rcx, rbx
    and cl, 31
    sar rax, cl
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    not rax
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

