#include "../registry.hpp"

#include "FN_types.hpp"
#include "FN_functions.hpp"

#include "RNA_access.h"

namespace FN {
namespace DataFlowNodes {

using BLI::float3;

static void LOAD_float(PointerRNA *rna, Tuple &tuple, uint index)
{
  float value = RNA_float_get(rna, "value");
  tuple.set<float>(index, value);
}

static void LOAD_vector(PointerRNA *rna, Tuple &tuple, uint index)
{
  float vector[3];
  RNA_float_get_array(rna, "value", vector);
  tuple.set<float3>(index, float3(vector));
}

static void LOAD_integer(PointerRNA *rna, Tuple &tuple, uint index)
{
  int value = RNA_int_get(rna, "value");
  tuple.set<int32_t>(index, value);
}

static void LOAD_boolean(PointerRNA *rna, Tuple &tuple, uint index)
{
  bool value = RNA_boolean_get(rna, "value");
  tuple.set<bool>(index, value);
}

template<typename T> static void LOAD_empty_list(PointerRNA *UNUSED(rna), Tuple &tuple, uint index)
{
  auto list = Types::SharedList<T>::New();
  tuple.move_in(index, list);
}

void initialize_socket_inserters(GraphInserters &inserters)
{
  inserters.reg_socket_loader("Float", LOAD_float);
  inserters.reg_socket_loader("Vector", LOAD_vector);
  inserters.reg_socket_loader("Integer", LOAD_integer);
  inserters.reg_socket_loader("Boolean", LOAD_boolean);
  inserters.reg_socket_loader("Float List", LOAD_empty_list<float>);
  inserters.reg_socket_loader("Vector List", LOAD_empty_list<float3>);
  inserters.reg_socket_loader("Integer List", LOAD_empty_list<int32_t>);
  inserters.reg_socket_loader("Boolean List", LOAD_empty_list<bool>);
}

}  // namespace DataFlowNodes
}  // namespace FN
