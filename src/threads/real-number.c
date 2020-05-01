//
// Created by sjtu-ypm on 19-3-6.
//

#include <lib/stdint.h>
#include <lib/stdio.h>
#include "real-number.h"
/*
 * A 32bit fixed point real number class, using int32.
 * The lowest 14 bits are fractional bits, the next 17 bits are the integer bits, the highest bit is sign bit.
 * It can donate form -131,071.999 to 131,071.999, the Accuracy is about 1e-4 ~ 1e-5.
 */
const F = 1 << 14;

/*  */
struct real_num_fixed32 real_num_fixed32_init(int32_t x){
    struct real_num_fixed32 res = {x * F};
    return res;
}

void real_num_fixed32_output(struct real_num_fixed32 a){
    a.num = a.num * 1000;
    int num = real_num_fixed32_round(a);
    printf("%d.%03d", num / 1000, num % 1000);
}

int32_t real_num_fixed32_trunc(struct real_num_fixed32 a){
    return a.num >> 14;
}

int32_t real_num_fixed32_round(struct real_num_fixed32 a){
    return (a.num >= 0) ? (a.num + (F >> 1)) / F: (a.num - (F >> 1)) / F;
}

struct real_num_fixed32 real_num_fixed32_add(struct real_num_fixed32 a, struct real_num_fixed32 b){
    struct real_num_fixed32 res = {a.num + b.num};
    return res;
}

struct real_num_fixed32 real_num_fixed32_add_int(struct real_num_fixed32 a, int32_t b){
    struct real_num_fixed32 res = {a.num + b * F};
    return res;
}

struct real_num_fixed32 real_num_fixed32_sub(struct real_num_fixed32 a, struct real_num_fixed32 b){
    struct real_num_fixed32 res = {a.num - b.num};
    return res;
}

struct real_num_fixed32 real_num_fixed32_sub_int(struct real_num_fixed32 a, int32_t b){
    struct real_num_fixed32 res = {a.num - b * F};
    return res;
}

struct real_num_fixed32 real_num_fixed32_mul(struct real_num_fixed32 a, struct real_num_fixed32 b){
    struct real_num_fixed32 res = {(int64_t)a.num * b.num / F};
    return res;
}

struct real_num_fixed32 real_num_fixed32_mul_int(struct real_num_fixed32 a, int32_t b){
    struct real_num_fixed32 res = {a.num * b};
    return res;
}

struct real_num_fixed32 real_num_fixed32_div(struct real_num_fixed32 a, struct real_num_fixed32 b){
    struct real_num_fixed32 res = {(int64_t)a.num * F / b.num};
    return res;
}

struct real_num_fixed32 real_num_fixed32_div_int(struct real_num_fixed32 a, int32_t b){
    struct real_num_fixed32 res = {a.num / b};
    return res;
}

struct real_num_fixed32 real_num_fixed32_div_int2(int32_t a, int32_t b){
    struct real_num_fixed32 res = {a * F / b};
    return res;
}

bool real_num_fixed32_cmp(struct real_num_fixed32 a, struct real_num_fixed32 b){
    return a.num < b.num;
}