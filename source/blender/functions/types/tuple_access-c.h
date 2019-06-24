#ifndef __FUNCTIONS_TYPES_TUPLE_ACCESS_C_H__
#define __FUNCTIONS_TYPES_TUPLE_ACCESS_C_H__

#include "FN_tuple-c.h"
#include "types-c.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

void FN_tuple_set_float(FnTuple tuple, uint index, float value);
void FN_tuple_set_int32(FnTuple tuple, uint index, int32_t value);
void FN_tuple_set_fvec3(FnTuple tuple, uint index, float vector[3]);
float FN_tuple_get_float(FnTuple tuple, uint index);
int32_t FN_tuple_get_int32(FnTuple tuple, uint index);
void FN_tuple_get_fvec3(FnTuple tuple, uint index, float dst[3]);
FnFloatList FN_tuple_relocate_out_float_list(FnTuple tuple, uint index);
FnFVec3List FN_tuple_relocate_out_fvec3_list(FnTuple tuple, uint index);

#ifdef __cplusplus
}
#endif

#endif /* __FUNCTIONS_TYPES_TUPLE_ACCESS_C_H__ */
