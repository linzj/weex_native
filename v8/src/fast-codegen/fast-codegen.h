#ifndef FAST_CODEGEN_H_
#define FAST_CODEGEN_H_
#include <memory>
#include "src/handles.h"
#include "src/interpreter/bytecode-register.h"
#include "src/macro-assembler.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {
namespace interpreter {
class BytecodeArrayIterator;
}

class CompilationInfo;
class Code;
class LabelRecorder;
class Callable;

class FastCodeGenerator {
 public:
  explicit FastCodeGenerator(CompilationInfo* info);
  ~FastCodeGenerator();

  CompilationInfo* info() { return info_; }
  Handle<Code> Generate();
  inline Isolate* isolate() { return isolate_; }

 private:
  void GeneratePrologue();
  void GenerateBody();
  void GenerateEpilogue();
#define BYTECODE_VISIT(name, ...) void Visit##name();
  BYTECODE_LIST(BYTECODE_VISIT)
#undef BYTECODE_VISIT
  interpreter::BytecodeArrayIterator& bytecode_iterator() const {
    return *bytecode_iterator_.get();
  }

  void LoadFixedArrayElement(Register array, Register to, uint32_t index,
                             int additional_offset = 0);
  void LoadFixedArrayElementSmiIndex(Register array, Register to,
                                     Register index, int additional_offset);
  void LoadWeakCellValueUnchecked(Register weak_cell, Register to);
  void LoadFeedbackVector(Register out);
  void LoadRegister(const interpreter::Register& r, Register out);
  void StoreRegister(const interpreter::Register& r, Register in);
  void GetContext(Register out);
  void SetContext(Register in);
  void LoadWeakCellValue(Register weak_cell, Register to, Label* if_cleared);
  void BuildLoadGlobal(Register out, int slot_operand_index,
                       int name_operand_index, TypeofMode typeof_mode);
  void DoLdaLookupSlot(Runtime::FunctionId function_id);
  void DoLdaLookupContextSlot(Runtime::FunctionId function_id);
  void DoLdaLookupGlobalSlot(Runtime::FunctionId function_id);
  void DoStoreIC(const Callable& ic);
  void DoStaGlobal(const Callable& ic);
  void DoKeyedStoreIC(const Callable& ic);
  void DoJSCall(TailCallMode tail_call_mode);
  template <class BinaryOpCodeStub>
  void DoBinaryOp();
  void GotoIfHasContextExtensionUpToDepth(Register context, Register scatch1,
                                          Register scatch2, Register scatch3,
                                          uint32_t depth, Label* target);
  void DoStaLookupSlot(LanguageMode language_mode);

  void DoBitwiseBinaryOp(Token::Value bitwise_op);
  void TruncateToWord();
  void ChangeInt32ToTagged(Register result);
  void BranchIfToBooleanIsTrue(Label* if_true, Label* if_false);
  void DoDelete(Runtime::FunctionId function_id);
  void DoLoadField(Register receiver, int handler_word);
  void HandleSmiCase(const Register& receiver, const Register& receiver_map,
                     Object* feedback, Object* smi, Label* done, Label* next);
  void DoLoadConstant(Handle<Object> map, int handler_word);

  void HandleCase(const Register& receiver, const Register& receiver_map,
                  Object* feedback, Object* handler, Label* done, Label* next);

  MacroAssembler masm_;
  CompilationInfo* info_;
  Isolate* isolate_;
  Handle<BytecodeArray> bytecode_array_;
  Label return_;
  Label truncate_slow_;
  std::unique_ptr<interpreter::BytecodeArrayIterator> bytecode_iterator_;
  std::unique_ptr<LabelRecorder> label_recorder_;
  DISALLOW_COPY_AND_ASSIGN(FastCodeGenerator);  // NOLINT
};
}
}
#endif  // FAST_CODEGEN_H_
