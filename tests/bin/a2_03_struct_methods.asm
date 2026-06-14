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
astra_a2_03_struct_methods__Point__sum__a2_03_struct_methods__Point:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rsi, rdi
    lea rdi, [rbp - 8]
    mov rax, [rsi]
    mov [rdi], rax
    lea rax, [rbp - 8]
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    add rax, 4
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

astra_a2_03_struct_methods__Point__scaleX__a2_03_struct_methods__Point_int32:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rsi, rdi
    lea rdi, [rbp - 8]
    mov rax, [rsi]
    mov [rdi], rax
    mov rax, rsi
    lea rdi, [rbp - 12]
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 8]
    movsxd rax, dword [rax]
    push rax
    lea rax, [rbp - 12]
    movsxd rax, dword [rax]
    mov rbx, rax
    pop rax
    imul rax, rbx
    movsxd rax, eax
    jmp .Lreturn_1
.Lreturn_1:
    mov rsp, rbp
    pop rbp
    ret

main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rdi, [rbp - 8]
    push rdi
    push rdi
    mov rax, 3
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    push rdi
    add rdi, 4
    push rdi
    mov rax, 4
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    lea rax, [rbp - 8]
    push rax
    pop rdi
    call astra_a2_03_struct_methods__Point__sum__a2_03_struct_methods__Point
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    lea rax, [rbp - 8]
    push rax
    mov rax, 10
    movsxd rax, eax
    push rax
    pop rsi
    pop rdi
    call astra_a2_03_struct_methods__Point__scaleX__a2_03_struct_methods__Point_int32
    mov rdi, rax
    call astra_print_i32
    xor rax, rax
    mov rax, 0
    movsxd rax, eax
    jmp .Lreturn_2
.Lreturn_2:
    mov rsp, rbp
    pop rbp
    ret

