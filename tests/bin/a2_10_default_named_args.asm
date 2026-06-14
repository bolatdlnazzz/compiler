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
astra_a2_10_default_named_args__repeat__int32_int32:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, rdi
    lea rdi, [rbp - 4]
    movsxd rax, eax
    mov dword [rdi], eax
    mov rax, rsi
    lea rdi, [rbp - 8]
    movsxd rax, eax
    mov dword [rdi], eax
    lea rdi, [rbp - 12]
    push rdi
    mov rax, 0
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rdi, [rbp - 16]
    push rdi
    mov rax, 0
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
.Lwhile_cond_1:
    lea rax, [rbp - 12]
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    setl al
    movzx rax, al
    cmp rax, 0
    je .Lwhile_end_2
    lea rax, [rbp - 16]
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    push rax
    lea rax, [rbp - 16]
    mov rdi, rax
    pop rax
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 12]
    movsxd rax, dword [rax]
    push rax
    mov rax, 1
    movsxd rax, eax
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    push rax
    lea rax, [rbp - 12]
    mov rdi, rax
    pop rax
    movsxd rax, eax
    mov dword [rdi], eax
    jmp .Lwhile_cond_1
.Lwhile_end_2:
    lea rax, [rbp - 16]
    movsxd rax, dword [rax]
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

main:
    push rbp
    mov rbp, rsp
    mov rax, 5
    movsxd rax, eax
    push rax
    mov rax, 2
    movsxd rax, eax
    push rax
    pop rsi
    pop rdi
    call astra_a2_10_default_named_args__repeat__int32_int32
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 7
    movsxd rax, eax
    push rax
    mov rax, 2
    movsxd rax, eax
    push rax
    pop rsi
    pop rdi
    call astra_a2_10_default_named_args__repeat__int32_int32
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 3
    movsxd rax, eax
    push rax
    mov rax, 4
    movsxd rax, eax
    push rax
    pop rsi
    pop rdi
    call astra_a2_10_default_named_args__repeat__int32_int32
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 2
    movsxd rax, eax
    push rax
    mov rax, 5
    movsxd rax, eax
    push rax
    pop rsi
    pop rdi
    call astra_a2_10_default_named_args__repeat__int32_int32
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_3
.Lreturn_3:
    mov rsp, rbp
    pop rbp
    ret

