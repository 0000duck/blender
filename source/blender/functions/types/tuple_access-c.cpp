#include "FN_types.hpp"

using namespace FN;
using namespace FN::Types;

void FN_tuple_set_float(FnTuple tuple_c, uint index, float value)
{
  unwrap(tuple_c)->set<float>(index, value);
}

float FN_tuple_get_float(FnTuple tuple_c, uint index)
{
  return unwrap(tuple_c)->get<float>(index);
}

void FN_tuple_set_int32(FnTuple tuple_c, uint index, int32_t value)
{
  unwrap(tuple_c)->set<int32_t>(index, value);
}

int32_t FN_tuple_get_int32(FnTuple tuple_c, uint index)
{
  return unwrap(tuple_c)->get<int32_t>(index);
}

void FN_tuple_set_float3(FnTuple tuple_c, uint index, float value[3])
{
  unwrap(tuple_c)->set<float3>(index, value);
}

void FN_tuple_get_float3(FnTuple tuple_c, uint index, float dst[3])
{
  *(float3 *)dst = unwrap(tuple_c)->get<float3>(index);
}

FnFloatList FN_tuple_relocate_out_float_list(FnTuple tuple_c, uint index)
{
  auto list = unwrap(tuple_c)->relocate_out<SharedFloatList>(index);
  return wrap(list.extract_ptr());
}

FnFloat3List FN_tuple_relocate_out_float3_list(FnTuple tuple_c, uint index)
{
  auto list = unwrap(tuple_c)->relocate_out<SharedFloat3List>(index);
  return wrap(list.extract_ptr());
}
