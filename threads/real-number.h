//
// Created by sjtu-ypm on 19-3-6.
//

#ifndef MYPINTOS_REAL_NUMBER_H
#define MYPINTOS_REAL_NUMBER_H

struct real_num_fixed32
{
    int32_t  num;
};
/* Transfer int32_t to real_num_fixed32 */
struct real_num_fixed32 real_num_fixed32_init(int32_t);

/* Print a real_num_fixed32 with 3 decimal */
void real_num_fixed32_output(struct real_num_fixed32);

/* Get the nearest integer of the real_num_fixed32 number
 * Trunc and round have the same definition with trunc() and round() in c++ std lib
 * */
int32_t real_num_fixed32_trunc(struct real_num_fixed32);
int32_t real_num_fixed32_round(struct real_num_fixed32);

/* Implemented
 * +,-,*,/ between two real_num_fixed32,
 * +,-,*,/ between a real_num_fixed32 and a int,
 * / between two int.
 * */
struct real_num_fixed32 real_num_fixed32_add(struct real_num_fixed32, struct real_num_fixed32);
struct real_num_fixed32 real_num_fixed32_add_int(struct real_num_fixed32, int32_t);
struct real_num_fixed32 real_num_fixed32_sub(struct real_num_fixed32, struct real_num_fixed32);
struct real_num_fixed32 real_num_fixed32_sub_int(struct real_num_fixed32, int32_t);
struct real_num_fixed32 real_num_fixed32_mul(struct real_num_fixed32, struct real_num_fixed32);
struct real_num_fixed32 real_num_fixed32_mul_int(struct real_num_fixed32, int32_t);
struct real_num_fixed32 real_num_fixed32_div(struct real_num_fixed32, struct real_num_fixed32);
struct real_num_fixed32 real_num_fixed32_div_int(struct real_num_fixed32, int32_t);
struct real_num_fixed32 real_num_fixed32_div_int2(int32_t, int32_t);

/* Compare two real_num_fixed32, return true if the first argument less then second argument */
bool real_num_fixed32_cmp(struct real_num_fixed32, struct real_num_fixed32);


#endif //MYPINTOS_REAL_NUMBER_H
