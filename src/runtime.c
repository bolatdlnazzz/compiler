#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void astra_print_i32(int32_t value) { printf("%d\n", value); }
void astra_print_i64(int64_t value) { printf("%lld\n", (long long)value); }
void astra_print_u32(uint32_t value) { printf("%u\n", value); }
void astra_print_u64(uint64_t value) { printf("%llu\n", (unsigned long long)value); }
void astra_print_f32(float value) { printf("%g\n", value); }
void astra_print_f64(double value) { printf("%g\n", value); }
void astra_print_bool(int64_t value) { printf(value ? "true\n" : "false\n"); }
void astra_print_string(const char* value) { printf("%s\n", value ? value : ""); }

char* astra_input_string(void) {
    char buffer[4096];
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') buffer[--len] = '\0';
    char* result = (char*)malloc(len + 1);
    if (!result) abort();
    memcpy(result, buffer, len + 1);
    return result;
}

void astra_exit(int32_t code) { exit(code); }

void astra_panic(const char* message) {
    fprintf(stderr, "runtime error: %s\n", message ? message : "panic");
    exit(1);
}

char* astra_string_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char* r = (char*)malloc(la + lb + 1);
    if (!r) abort();
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}

int64_t astra_string_eq(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}

int64_t astra_string_ne(const char* a, const char* b) { return !astra_string_eq(a, b); }
int32_t astra_string_len(const char* value) { return value ? (int32_t)strlen(value) : 0; }

void astra_rt_div_zero(int32_t line) {
    fprintf(stderr, "runtime error: division by zero at line %d\n", line);
    exit(1);
}

void astra_rt_oob(int32_t line) {
    fprintf(stderr, "runtime error: array index out of bounds at line %d\n", line);
    exit(1);
}
