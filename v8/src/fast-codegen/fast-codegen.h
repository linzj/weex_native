#ifndef FAST_CODEGEN_H
#define FAST_CODEGEN_H 
#include "src/handles.h"
#include "src/macro-assembler.h"
#include "src/interpreter/bytecode-register.h"

namespace v8 {
namespace internal {

class CompilationInfo;
class Code;
class FastCodeGenerator {
 public:
  explicit FastCodeGenerator(CompilationInfo* info);

  CompilationInfo* info() { return info_; }
  Handle<Code> Generate();
 private:
  void GeneratePrologue();
  void GenerateBody();
  void GenerateEpilogue();
#define BYTECODE_VISIT(name, ...)       \
  void Visit##name();
        BYTECODE_LIST(BYTECODE_VISIT)
#undef BYTECODE_VISIT
  const interpreter::BytecodeArrayIterator& bytecode_iterator() const {
    return *bytecode_iterator_;
  }

  void set_bytecode_iterator(
      const interpreter::BytecodeArrayIterator* bytecode_iterator) {
    bytecode_iterator_ = bytecode_iterator;
  }

  void LoadConstantPoolEntry(uint32_t value, Register to);
  void LoadFixedArrayElement(Register array, Register to, uint32_t index, int additional_offset = 0);
  void LoadFeedbackVector(Register out);
  void LoadRegister(const interpreter::Register& r, Register out);
  void StoreRegister(const interpreter::Register& r, Register in);
  void GetContext(Register out);
  void SetContext(Register in);
  void LoadWeakCellValue(Register weak_cell, Register to, Label* if_cleared);
  void BuildLoadGlobal(Register out, int slot_operand_index,
                                  int name_operand_index,
                                  TypeofMode typeof_mode);
  void DoLdaLookupSlot(Runtime::FunctionId function_id);
  void DoLdaLookupContextSlot(Runtime::FunctionId function_id);
  void DoLdaLookupGlobalSlot(Runtime::FunctionId function_id);
  void DoStoreIC(Callable ic);
  void DoKeyedStoreIC(Callable ic);
  void DoJSCall(TailCallMode tail_call_mode);
  template <class BinaryOpCodeStub>
  void DoBinaryOp();

  void DoBitwiseBinaryOp(Token::Value bitwise_op);
  void TruncateToWord(Register in_and_out);
  void ChangeInt32ToTagged(Register result);
  void BranchIfToBooleanIsTrue(Label* if_true,
          Label* if_false);
  void DoDelete(Runtime::FunctionId function_id);

  MacroAssembler masm_;
  CompilationInfo* info_;
  Isolate* isolate_;
  Handle<BytecodeArray> byte_code_array_;
  Label return_;
  Label truncate_slow_;
  const interpreter::BytecodeArrayIterator* bytecode_iterator_;
};


FastCodeGenerator::FastCodeGenerator(CompilationInfo* info): info_(info) {}

}
}
#endif  // FAST_CODEGEN_H
