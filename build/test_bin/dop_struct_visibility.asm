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
astra_demo__Counter__hidden__demo__Counter:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rsi, rdi
    lea rdi, [rbp - 4]
    mov eax, dword [rsi]
    mov dword [rdi], eax
    lea rax, [rbp - 4]
    movsxd rax, dword [rax]
    push rax
    mov rax, 1
    movsxd rax, eax
    mov rbx, rax
    pop rax
    add rax, rbx
    movsxd rax, eax
    jmp .Lreturn_0
.Lreturn_0:
    mov rsp, rbp
    pop rbp
    ret

astra_demo__Counter__show__demo__Counter:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rsi, rdi
    lea rdi, [rbp - 4]
    mov eax, dword [rsi]
    mov dword [rdi], eax
    lea rax, [rbp - 4]
    push rax
    pop rdi
    call astra_demo__Counter__hidden__demo__Counter
    jmp .Lreturn_1
.Lreturn_1:
    mov rsp, rbp
    pop rbp
    ret

main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rdi, [rbp - 4]
    push rdi
    push rdi
    mov rax, 41
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    pop rdi
    lea rax, [rbp - 4]
    push rax
    pop rdi
    call astra_demo__Counter__show__demo__Counter
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

