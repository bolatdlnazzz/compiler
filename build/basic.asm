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
astra_square:
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
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    imul rax, rbx
    movsxd rax, eax
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

main:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    lea rdi, [rbp - 4]
    push rdi
    mov rax, 5
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    pop rdi
    call astra_square
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rdi, [rbp - 16]
    push rdi
    add rdi, 0
    push rdi
    mov rax, 10
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    push rdi
    add rdi, 4
    push rdi
    mov rax, 20
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    push rdi
    add rdi, 8
    push rdi
    mov rax, 30
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    lea rax, [rbp - 16]
    push rax
    mov rax, 1
    movsxd rax, eax
    cmp rax, 0
    jl .Lidx_ok_2_bad
    cmp rax, 3
    jge .Lidx_ok_2_bad
    jmp .Lidx_ok_2
.Lidx_ok_2_bad:
    mov rdi, 10
    sub rsp, 8
    call astra_rt_oob
    add rsp, 8
.Lidx_ok_2:
    imul rax, 4
    pop rbx
    add rax, rbx
    movsxd rax, dword [rax]
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    mov rax, 3
    movsxd rax, eax
    mov rbx, rax
    pop rax
    cmp rax, rbx
    setg al
    movzx rax, al
    cmp rax, 0
    je .Lelse_3
    mov rax, 100
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    jmp .Lendif_4
.Lelse_3:
    mov rax, 0
    movsxd rax, eax
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
.Lendif_4:
    lea rdi, [rbp - 20]
    push rdi
    mov rax, 0
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
.Lwhile_cond_5:
    lea rax, [rbp - 20]
    movsxd rax, dword [rax]
    push rax
    mov rax, 3
    movsxd rax, eax
    mov rbx, rax
    pop rax
    cmp rax, rbx
    setl al
    movzx rax, al
    cmp rax, 0
    je .Lwhile_end_6
    lea rax, [rbp - 20]
    movsxd rax, dword [rax]
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 20]
    movsxd rax, dword [rax]
    push rax
    mov rax, 1
    movsxd rax, eax
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    push rax
    lea rax, [rbp - 20]
    mov rdi, rax
    pop rax
    movsxd rax, eax
    mov dword [rdi], eax
    jmp .Lwhile_cond_5
.Lwhile_end_6:
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_1
.Lreturn_1:
    mov rsp, rbp
    pop rbp
    ret

