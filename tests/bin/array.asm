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
    lea rdi, [rbp - 12]
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
    mov rax, 3
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 12]
    push rax
    mov rax, 1
    movsxd rax, eax
    cmp rax, 0
    jl .Lidx_ok_1_bad
    cmp rax, 3
    jge .Lidx_ok_1_bad
    jmp .Lidx_ok_1
.Lidx_ok_1_bad:
    mov rdi, 5
    sub rsp, 8
    call astra_rt_oob
    add rsp, 8
.Lidx_ok_1:
    imul rax, 4
    pop rbx
    add rax, rbx
    movsxd rax, dword [rax]
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 12]
    push rax
    mov rax, 0
    movsxd rax, eax
    cmp rax, 0
    jl .Lidx_ok_2_bad
    cmp rax, 3
    jge .Lidx_ok_2_bad
    jmp .Lidx_ok_2
.Lidx_ok_2_bad:
    mov rdi, 7
    sub rsp, 8
    call astra_rt_oob
    add rsp, 8
.Lidx_ok_2:
    imul rax, 4
    pop rbx
    add rax, rbx
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 12]
    push rax
    mov rax, 1
    movsxd rax, eax
    cmp rax, 0
    jl .Lidx_ok_3_bad
    cmp rax, 3
    jge .Lidx_ok_3_bad
    jmp .Lidx_ok_3
.Lidx_ok_3_bad:
    mov rdi, 7
    call astra_rt_oob
.Lidx_ok_3:
    imul rax, 4
    pop rbx
    add rax, rbx
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    push rax
    lea rax, [rbp - 12]
    push rax
    mov rax, 2
    movsxd rax, eax
    cmp rax, 0
    jl .Lidx_ok_4_bad
    cmp rax, 3
    jge .Lidx_ok_4_bad
    jmp .Lidx_ok_4
.Lidx_ok_4_bad:
    mov rdi, 7
    call astra_rt_oob
.Lidx_ok_4:
    imul rax, 4
    pop rbx
    add rax, rbx
    mov rdi, rax
    pop rax
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 12]
    push rax
    mov rax, 2
    movsxd rax, eax
    cmp rax, 0
    jl .Lidx_ok_5_bad
    cmp rax, 3
    jge .Lidx_ok_5_bad
    jmp .Lidx_ok_5
.Lidx_ok_5_bad:
    mov rdi, 9
    sub rsp, 8
    call astra_rt_oob
    add rsp, 8
.Lidx_ok_5:
    imul rax, 4
    pop rbx
    add rax, rbx
    movsxd rax, dword [rax]
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 12]
    push rax
    mov rax, 2
    movsxd rax, eax
    cmp rax, 0
    jl .Lidx_ok_6_bad
    cmp rax, 3
    jge .Lidx_ok_6_bad
    jmp .Lidx_ok_6
.Lidx_ok_6_bad:
    mov rdi, 11
    sub rsp, 8
    call astra_rt_oob
    add rsp, 8
.Lidx_ok_6:
    imul rax, 4
    pop rbx
    add rax, rbx
    movsxd rax, dword [rax]
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

