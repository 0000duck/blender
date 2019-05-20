#include "vectors.hpp"
#include "FN_types.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"
#include "BLI_lazy_init.hpp"
#include "BLI_math.h"

namespace FN {
namespace Functions {

using namespace Types;

class CombineVectorGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Type *vector_ty = get_llvm_type(GET_TYPE_fvec3(), builder.getContext());

    llvm::Value *vector = builder.getUndef(vector_ty);
    vector = builder.CreateInsertElement(vector, interface.get_input(0), 0);
    vector = builder.CreateInsertElement(vector, interface.get_input(1), 1);
    vector = builder.CreateInsertElement(vector, interface.get_input(2), 2);
    interface.set_output(0, vector);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_combine_vector)
{
  FunctionBuilder builder;
  builder.add_input("X", GET_TYPE_float());
  builder.add_input("Y", GET_TYPE_float());
  builder.add_input("Z", GET_TYPE_float());
  builder.add_output("Vector", GET_TYPE_fvec3());

  auto fn = builder.build("Combine Vector");
  fn->add_body<CombineVectorGen>();
  return fn;
}

class SeparateVector : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *vector = interface.get_input(0);
    interface.set_output(0, builder.CreateExtractElement(vector, 0));
    interface.set_output(1, builder.CreateExtractElement(vector, 1));
    interface.set_output(2, builder.CreateExtractElement(vector, 2));
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_separate_vector)
{
  FunctionBuilder builder;
  builder.add_input("Vector", GET_TYPE_fvec3());
  builder.add_output("X", GET_TYPE_float());
  builder.add_output("Y", GET_TYPE_float());
  builder.add_output("Z", GET_TYPE_float());

  auto fn = builder.build("Separate Vector");
  fn->add_body<SeparateVector>();
  return fn;
}

class VectorDistance : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    Vector a = fn_in.get<Vector>(0);
    Vector b = fn_in.get<Vector>(1);
    float distance = len_v3v3((float *)&a, (float *)&b);
    fn_out.set<float>(0, distance);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_vector_distance)
{
  FunctionBuilder builder;
  builder.add_input("A", GET_TYPE_fvec3());
  builder.add_input("B", GET_TYPE_fvec3());
  builder.add_output("Distance", GET_TYPE_float());

  auto fn = builder.build("Vector Distance");
  fn->add_body<VectorDistance>();
  return fn;
}

static SharedFunction get_math_function__two_inputs(std::string name)
{
  FunctionBuilder builder;
  builder.add_input("A", GET_TYPE_fvec3());
  builder.add_input("B", GET_TYPE_fvec3());
  builder.add_output("Result", GET_TYPE_fvec3());
  return builder.build(name);
}

class AddVectors : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    Vector a = fn_in.get<Vector>(0);
    Vector b = fn_in.get<Vector>(1);
    Vector result(a.x + b.x, a.y + b.y, a.z + b.z);
    fn_out.set<Vector>(0, result);
  }
};

class AddVectorsGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *a = interface.get_input(0);
    llvm::Value *b = interface.get_input(1);
    llvm::Value *result = builder.CreateFAdd(a, b);
    interface.set_output(0, result);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_add_vectors)
{
  auto fn = get_math_function__two_inputs("Add Vectors");
  fn->add_body<AddVectors>();
  fn->add_body<AddVectorsGen>();
  return fn;
}

/* Constant vector builders
 *****************************************/

class ConstFVec3 : public TupleCallBody {
 private:
  Vector m_vector;

 public:
  ConstFVec3(Vector vector) : m_vector(vector)
  {
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    fn_out.set<Vector>(0, m_vector);
  }
};

class ConstFVec3Gen : public LLVMBuildIRBody {
 private:
  Vector m_vector;
  LLVMTypeInfo *m_type_info;

 public:
  ConstFVec3Gen(Vector vector) : m_vector(vector)
  {
    m_type_info = GET_TYPE_fvec3()->extension<LLVMTypeInfo>();
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *output = builder.getUndef(m_type_info->get_type(builder.getContext()));
    output = builder.CreateInsertElement(output, builder.getFloat(m_vector.x), 0);
    output = builder.CreateInsertElement(output, builder.getFloat(m_vector.y), 1);
    output = builder.CreateInsertElement(output, builder.getFloat(m_vector.z), 2);
    interface.set_output(0, output);
  }
};

static SharedFunction get_output_fvec3_function(Vector vector)
{
  FunctionBuilder builder;
  builder.add_output("Vector", GET_TYPE_fvec3());
  auto fn = builder.build("Build Vector");
  fn->add_body<ConstFVec3>(vector);
  fn->add_body<ConstFVec3Gen>(vector);
  return fn;
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_fvec3_0)
{
  return get_output_fvec3_function(Vector(0, 0, 0));
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_fvec3_1)
{
  return get_output_fvec3_function(Vector(1, 1, 1));
}

}  // namespace Functions
}  // namespace FN
