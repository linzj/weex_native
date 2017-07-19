#include "src/fast-codegen/fast-codegen.h"
#include "src/objects.h"
#include "src/code-factory.h"
#include "src/ic/accessor-assembler.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecodes.h"

#define __ masm_.

namespace v8 {
namespace internal {
Handle<Code> FastCodeGenerator::Generate() {
  DCHECK_EQ(kInterpreterAccumulatorRegister, r0);
  Isolate* isolate = info->isolate();
  isolate_ = isolate;
  HandleScope handle_scope(isolate);
  Object* maybe_byte_code_array = info->shared_info()->function_data();
  DCHECK(maybe_byte_code_array->IsBytecodeArray());
  byte_code_array_ = handle(BytecodeArray::cast(maybe_byte_code_array));
  GeneratePrologue();
  GenerateBody();
  GenerateEpilogue();

  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, flags, masm.CodeObject());
  return handle_scope.CloseAndEscape(code());
}


// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r1: the JS function object being called.
//   o r3: the new target
//   o cp: our context
//   o pp: the caller's constant pool pointer (if enabled)
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void FastCodeGenerator::GeneratePrologue() {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(r1);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ ldr(r0, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  Register debug_info = kInterpreterBytecodeArrayRegister;
  DCHECK(!debug_info.is(r0));
  __ ldr(debug_info, FieldMemOperand(r0, SharedFunctionInfo::kDebugInfoOffset));
  __ SmiTst(debug_info);
  // Load original bytecode array or the debug copy.
  __ ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(r0, SharedFunctionInfo::kFunctionDataOffset), eq);
  __ ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex), ne);
  // Load the initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push new.target, bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r0, kInterpreterBytecodeOffsetRegister);
  __ Push(r3, kInterpreterBytecodeArrayRegister, r0);


  // Allocate the local and temporary register file on the stack.
  {
    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ sub(r9, sp, Operand(byte_code_array_->frame_size()));
    __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
    __ cmp(r9, Operand(r2));
    __ b(hs, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
    for (int i = 0; i < byte_code_array_->frame_size(); ++i)
      __ push(r9);
  }

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
}

void FastCodeGenerator::GenerateBody() {
  for (; !iterator.done(); iterator.Advance()) {
    switch (iterator.current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name();                     \
    break;
        BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CASE
    }
  }
}


void FastCodeGenerator::GenerateEpilogue() {
  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments.
  __ add(sp, sp, byte_code_array_->parameter_count() << kPointerSizeLog2, LeaveCC);
  __ Jump(lr);
}

void FastCodeGenerator::VisitLdaZero() {
  __ mov(kInterpreterAccumulatorRegister, Operand(0));
}

void FastCodeGenerator::VisitLdaSmi() {
  __ Move(kInterpreterAccumulatorRegister, Smi::FromInt(bytecode_iterator().GetImmediateOperand(0)));
}

uint32_t CodeStubAssembler::ElementOffsetFromIndex(uint32_t index,
                                                ElementsKind kind,
                                                int base_size) {
  int element_size_shift = ElementsKindToShiftSize(kind);
  int element_size = 1 << element_size_shift;
  int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
  return (base_size + element_size * index);
}

void FastCodeGenerator::LoadFixedArrayElement(Register array, Register to, uint32_t index, int additional_offset) {
  int32_t header_size =
      FixedArray::kHeaderSize + additional_offset;
  uint32_t offset = ElementOffsetFromIndex(index, FAST_HOLEY_ELEMENTS,
                                        parameter_mode, header_size);
  return __ Load(to, FieldMemOperand(array, offset), Representation::HeapObject());

}

void FastCodeGenerator::LoadConstantPoolEntry(uint32_t index, Register to) {
  to = __ Load(to, FieldMemOperand(kInterpreterBytecodeArrayRegister, BytecodeArray::kConstantPoolOffset), Representation::HeapObject);
  LoadFixedArrayElement(to, to, index);
}

void FastCodeGenerator::VisitLdaConstant() {
  uint32_t index = bytecode_iterator().GetIndexOperand(0);
  FixedArray* constant_pool = byte_code_array_->constant_pool();
  Object* constant = constant_pool->get(index);
  DCHECK(constant->IsHeapObject());
  __ mov(kInterpreterAccumulatorRegister, Operand(handle(HeapObject::cast(constant))));
}

void FastCodeGenerator::VisitLdaUndefined() {
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
}

void FastCodeGenerator::VisitLdaNull() {
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kNullValueRootIndex);
}

void FastCodeGenerator::VisitLdaTheHole() {
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kTheHoleValueRootIndex);
}

void FastCodeGenerator::VisitLdaTrue() {
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kTrueValueRootIndex);
}

void FastCodeGenerator::VisitLdaFalse() {
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kFalseValueRootIndex);
}

void FastCodeGenerator::VisitLdar() {
  auto reg = bytecode_iterator().GetRegisterOperand(0);
  __ ldr(kInterpreterAccumulatorRegister, MemOperand(fp, reg.ToOperand() << kPointerSizeLog2));
}

void FastCodeGenerator::VisitStar() {
  auto reg = bytecode_iterator().GetRegisterOperand(0);
  __ str(kInterpreterAccumulatorRegister, MemOperand(fp, reg.ToOperand() << kPointerSizeLog2));
}

void FastCodeGenerator::VisitMov() {
  auto reg0 = bytecode_iterator().GetRegisterOperand(0);
  auto reg1 = bytecode_iterator().GetRegisterOperand(1);

  __ ldr(r1, MemOperand(fp, reg0.ToOperand() << kPointerSizeLog2));
  __ str(r1, MemOperand(fp, reg1.ToOperand() << kPointerSizeLog2));
}

void FastCodeGenerator::LoadRegister(const interpreter::Register& r, Register out) {
  __ ldr(out, MemOperand(fp, r.ToOperand() << kPointerSizeLog2));
}

void FastCodeGenerator::StoreRegister(const interpreter::Register& r, Register in) {
  __ str(out, MemOperand(fp, r.ToOperand() << kPointerSizeLog2));
}

void FastCodeGenerator::LoadFeedbackVector(Register out) {
  LoadRegister(interpreter::Register::function_closure(), out);
  __ ldr(out, FieldMemOperand(out, JSFunction::kFeedbackVectorOffset));
  __ ldr(out, FieldMemOperand(out, Cell::kValueOffset));
}

#define LoadObjectField(from, to, offset) __ ldr(to, FieldMemOperand(from, offset))

void FastCodeGenerator::LoadWeakCellValueUnchecked(Register weak_cell, Register to) {
  LoadObjectField(weak_cell, to, WeakCell::kValueOffset);
}

void FastCodeGenerator::LoadWeakCellValue(Register weak_cell, Register to, Label* if_cleared) {
  LoadWeakCellValueUnchecked(weak_cell, to);
  if (if_cleared != nullptr) {
    __ cmp(to, Operand(0));
    __ b(eq, if_cleared);
  }
}

void FastCodeGenerator::GetContext(Register out) {
  __ mov(out, cp);
}

void FastCodeGenerator::SetContext(Register in) {
  __ mov(cp, in);
}

void FastCodeGenerator::BuildLoadGlobal(Register out, int slot_operand_index,
                                  int name_operand_index,
                                  TypeofMode typeof_mode) {
  Register feedback_vector_reg = r0;
  LoadFeedbackVector(feedback_vector_reg);
  uint32_t feedback_slot = bytecode_iterator().GetIndexOperand(slot_operand_index);

  Label try_handler, miss, done_try_property, done;

  // Fast path without frame construction for the data case.
  {
    Comment cmnt(&masm_, "LoadGlobalIC_TryPropertyCellCase");
    Register weakcell_reg = r0;
    Register value_reg = r0;
    LoadFixedArrayElement(feedback_vector_reg, weakcell_reg, feedback_slot, 0);

    // Load value or try handler case if the {weak_cell} is cleared.
    LoadWeakCellValue(weakcell_reg, value_reg, &try_handler);

    LoadObjectField(value_reg, out, PropertyCell::kValueOffset);
    __ JumpIfRoot(out, Heap::kTheHoleValueRootIndex, &miss);
    __ b(&done_try_property);
  }

  // Slow path with frame construction.
  {
    __ bind(&try_handler);
    // {
    //   GetContext(r1);
    //   Register symbol
    //   Handle<Object> name = bytecode_iterator().GetConstantForIndexOperand(name_operand_index);

    //   AccessorAssembler::LoadICParameters params(context, nullptr, name,
    //                                              smi_slot, feedback_vector);
    //   Label call_handler;

    //   Node* handler =
    //       LoadFixedArrayElement(feedback_vector, p->slot, kPointerSize, SMI_PARAMETERS);
    //   CSA_ASSERT(this, Word32BinaryNot(TaggedIsSmi(handler)));
    //   GotoIf(WordEqual(handler, LoadRoot(Heap::kuninitialized_symbolRootIndex)),
    //          miss);
    //   GotoIf(IsCodeMap(LoadMap(handler)), &call_handler);

    //   bool throw_reference_error_if_nonexistent = typeof_mode == NOT_INSIDE_TYPEOF;
    //   HandleLoadGlobalICHandlerCase(p, handler, miss, exit_point,
    //                                 throw_reference_error_if_nonexistent);

    //   Bind(&call_handler);
    //   {
    //     LoadWithVectorDescriptor descriptor(isolate());
    //     Node* native_context = LoadNativeContext(p->context);
    //     Node* receiver =
    //         LoadContextElement(native_context, Context::EXTENSION_INDEX);
    //     exit_point->ReturnCallStub(descriptor, handler, p->context, receiver,
    //                                p->name, p->slot, p->vector);
    //   }
    // }

    __ Bind(&miss);
    {
      Register context_reg = r1;
      Register name_reg = r1;
      Register smi_slot_reg = r1;
      GetContext(context_reg);
      __ push(context_reg);
      
      Handle<Object> name = bytecode_iterator().GetConstantForIndexOperand(name_operand_index);
      __ mov(name_reg, Operand(name));
      __ push(name);

      __ mov(smi_slot_reg, Operand(Smi::FromInt(feedback_slot)));
      __ push(smi_slot_reg);
      __ push(feedback_vector_reg);
      __ CallRuntime(Runtime::kLoadGlobalIC_Miss);
      DCHECK_EQ(out, r0);
    }

    __ bind(&done);
    __ bind(&done_try_property);
  }
}

void FastCodeGenerator::VisitLdaGlobal() {
  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  BuildLoadGlobal(kInterpreterAccumulatorRegister, kSlotOperandIndex, kNameOperandIndex, NOT_INSIDE_TYPEOF);
}

void FastCodeGenerator::VisitLdaGlobalInsideTypeof() {
  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  BuildLoadGlobal(kInterpreterAccumulatorRegister, kSlotOperandIndex, kNameOperandIndex, INSIDE_TYPEOF);
}


void FastCodeGenerator::DoStaGlobal(const Callable& ic) {
  __ LoadGlobalObject(StoreWithVectorDescriptor::ReceiverRegister());
  __ mov(StoreWithVectorDescriptor::NameRegister(), Operand(bytecode_iterator.GetConstantForIndexOperand(0)));
  DCHECK_EQ(StoreWithVectorDescriptor::ValueRegister(), kInterpreterAccumulatorRegister);
  __ mov(StoreWithVectorDescriptor::SlotRegister(), Operand(Smi::FromInt(bytecode_iterator.GetIndexOperand(1))));
  LoadFeedbackVector(StoreWithVectorDescriptor::VectorRegister());
  __ Call(ic.code());
}

void FastCodeGenerator::VisitStaGlobalSloppy() {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY);
  DoStaGlobal(ic);
}

void FastCodeGenerator::VisitStaGlobalStrict() {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, STRICT);
  DoStaGlobal(ic);
}

void FastCodeGenerator::VisitStaDataPropertyInLiteral() {
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), r1);
  __ push(r1);
  LoadRegister(bytecode_iterator().GetRegisterOperand(1), r1);
  __ push(r1);
  __ push(kInterpreterAccumulatorRegister);
  __ mov(r1, Operand(Smi::FromInt(bytecode_iterator().GetFlagOperand(2))));
  __ push(r1);
  __ mov(r1, Operand(Smi::FromInt(bytecode_iterator().GetIndexOperand(3))));
  __ push(r1);

  LoadFeedbackVector(r1);
  __ push(r1);

  __ CallRuntime(Runtime::kDefineDataPropertyInLiteral);
}

void FastCodeGenerator::VisitLdaContextSlot() {
  Register context_reg = r0;
  auto reg = bytecode_iterator().GetRegisterOperand(0);
  LoadRegister(context_reg, reg);
  __ LoadContext(context_reg, context_reg, bytecode_iterator().GetUnsignedImmediateOperand(2));
  __ ldr(kInterpreterAccumulatorRegister, ContextMemOperand(context_reg, bytecode_iterator().GetIndexOperand(1)));
}

void FastCodeGenerator::VisitLdaImmutableContextSlot() {
  VisitLdaContextSlot();
}

void FastCodeGenerator::VisitLdaCurrentContextSlot() {
  auto slot_index = bytecode_iterator.GetIndexOperand(0);
  GetContext(r1);
  __ ldr(kInterpreterAccumulatorRegister, ContextMemOperand(r1, slot_index));
}

void FastCodeGenerator::VisitLdaImmutableCurrentContextSlot() {
  VisitLdaCurrentContextSlot();
}

void FastCodeGenerator::VisitStaContextSlot() {
  auto reg = bytecode_iterator().GetRegisterOperand(0);
  LoadRegister(reg, r1);
  uint32_t slot_index = bytecode_iterator().GetIndexOperand(1);
  uint32_t depth = bytecode_iterator().GetUnsignedImmediateOperand(2);
  __ LoadContext(r1, r1, depth);
  __ str(kInterpreterAccumulatorRegister, ContextMemOperand(r1, slot_index));
}

void FastCodeGenerator::VisitStaCurrentContextSlot() {
  Node* value = __ GetAccumulator();
  Node* slot_index = __ BytecodeOperandIdx(0);
  Node* slot_context = __ GetContext();
  uint32_t slot_context = bytecode_iterator().GetIndexOperand(0);
  GetContext(r1);
  __ str(kInterpreterAccumulatorRegister, ContextMemOperand(r1, slot_index));
}

void FastCodeGenerator::DoLdaLookupSlot(Runtime::FunctionId function_id) {
  Handle<Object> name = bytecode_iterator().GetConstantForIndexOperand(0);
  __ mov(r1, Operand(name));
  __ push(r1);
  __ CallRuntime(function_id);
}

void FastCodeGenerator::VisitLdaLookupSlot() {
  DoLdaLookupSlot(Runtime::kLoadLookupSlot);
}

void FastCodeGenerator::VisitLdaLookupSlotInsideTypeof() {
  DoLdaLookupSlot(Runtime::kLoadLookupSlotInsideTypeof);
}

void FastCodeGenerator::GotoIfHasContextExtensionUpToDepth(Register context,
                                                           Register scatch1,
                                                           Register scatch2,
                                                           Register scatch3,
                                                              uint32_t depth,
                                                              Label* target) {
  Label context_search;
  __ LoadRoot(scatch2, Heap::kTheHoleValueRootIndex);
  __ mov(scatch3, Operand(depth));
  __ bind(&context_search); 
  {
    __ ldr(scatch1, ContextMemOperand(context, Context::EXTENSION_INDEX));
    __ cmp(scatch1, scatch2);
    __ b(target, ne);
    __ sub(scatch3, scatch3, Operand(1), SetCC);
    __ ldr(context, ContextMemOperand(context, Context::PREVIOUS_INDEX));
    __ b(&context_search, ne);
  }
}


void FastCodeGenerator::DoLdaLookupContextSlot(Runtime::FunctionId function_id) {
  Register context_reg = r1;
  uint32_t slot_index = bytecode_iterator().GetIndexOperand(1);
  uint32_t depth = bytecode_iterator().GetUnsignedImmediateOperand(2);
  Label slowpath, done;
  GetContext(context_reg);
  GotoIfHasContextExtensionUpToDepth(context_reg, r2, r3, r4, depth, &slowpath);
  GetContext(context_reg);
  __ LoadContext(context, context, depth);
  __ ldr(kInterpreterAccumulatorRegister, ContextMemOperand(context, slot_index));
  __ b(&done);
  __ bind(&slowpath);
  Handle<Object> name = bytecode_iterator.GetConstantForIndexOperand(0);
  __ mov(r1, Operand(name));
  __ push(r1);
  __ CallRuntime(function_id);
  __ bind(&done);
}


void FastCodeGenerator::VisitLdaLookupContextSlot() {
  DoLdaLookupContextSlot(Runtime::kLoadLookupSlot);
}

void FastCodeGenerator::VisitLdaLookupContextSlotInsideTypeof() {
  DoLdaLookupContextSlot(Runtime::kLoadLookupSlotInsideTypeof);
}

void FastCodeGenerator::DoLdaLookupGlobalSlot(Runtime::FunctionId function_id) {
  Register context_reg = r1;
  uint32_t depth = bytecode_iterator.GetUnsignedImmediateOperand(2);
  GetContext(context_reg);
  Label slowpath, done;
  GotoIfHasContextExtensionUpToDepth(context_reg, r2, r3, r4, depth, &slowpath);

  {
    static const int kNameOperandIndex = 0;
    static const int kSlotOperandIndex = 1;

    TypeofMode typeof_mode = function_id == Runtime::kLoadLookupSlotInsideTypeof
                                 ? INSIDE_TYPEOF
                                 : NOT_INSIDE_TYPEOF;

    BuildLoadGlobal(kInterpreterAccumulatorRegister, kSlotOperandIndex, kNameOperandIndex, typeof_mode);
    __ b(&done);
  }
  __bind(&slowpath);
  Handle<Object> name = bytecode_iterator().GetConstantForIndexOperand(0);
  __ mov(r1, Operand(name));
  __ push(r1);
  __ CallRuntime(function_id);
  __ bind(&done);
}

void FastCodeGenerator::VisitLdaLookupGlobalSlot() {
  DoLdaLookupGlobalSlot(Runtime::kLoadLookupSlot);
}

void FastCodeGenerator::VisitLdaLookupGlobalSlotInsideTypeof() {
  DoLdaLookupGlobalSlot(Runtime::kLoadLookupSlotInsideTypeof);
}

void FastCodeGenerator::DoStaLookupSlot(LanguageMode language_mode) {
  Node* value = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* name = __ LoadConstantPoolEntry(index);
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(is_strict(language_mode)
                                    ? Runtime::kStoreLookupSlot_Strict
                                    : Runtime::kStoreLookupSlot_Sloppy,
                                context, name, value);
  Handle<Object> name = bytecode_iterator().GetConstantForIndexOperand(0);
  __ mov(r1, Operand(name));
  __ Push(r1, kInterpreterAccumulatorRegister);
  __ CallRuntime(is_strict(language_mode)
                                    ? Runtime::kStoreLookupSlot_Strict
                                    : Runtime::kStoreLookupSlot_Sloppy);
}

void FastCodeGenerator::VisitStaLookupSlotSloppy() {
  DoStaLookupSlot(LanguageMode::SLOPPY);
}

void FastCodeGenerator::VisitStaLookupSlotStrict() {
  DoStaLookupSlot(LanguageMode::STRICT);
}

void FastCodeGenerator::VisitLdaNamedProperty() {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_);
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), LoadWithVectorDescriptor::ReceiverRegister());
  Handle<Object> name = bytecode_iterator.GetConstantForIndexOperand(1);
  __ mov(LoadWithVectorDescriptor::NameRegister(), Operand(name));
  __ mov(LoadWithVectorDescriptor::SlotRegister(), Operand(Smi::FromInt(bytecode_iterator().GetIndexOperand(2))));
  LoadFeedbackVector(LoadFeedbackVector::VectorRegister());
  __ Call(ic.code());
}

void FastCodeGenerator::VisitLdaKeyedProperty() {
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), LoadWithVectorDescriptor::ReceiverRegister());
  __ mov(LoadWithVectorDescriptor::NameRegister(), kInterpreterAccumulatorRegister);
  __ mov(LoadWithVectorDescriptor::SlotRegister(), Operand(Smi::FromInt(bytecode_iterator().GetIndexOperand(2))));
  LoadFeedbackVector(LoadWithVectorDescriptor::VectorRegister());
  __ Call(ic.code());
}


void FastCodeGenerator::DoStoreIC(Callable ic) {
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), StoreWithVectorDescriptor::ReceiverRegister());
  Handle<Object> name = bytecode_iterator.GetConstantForIndexOperand(1);
  __ mov(StoreWithVectorDescriptor::NameRegister(), Operand(name));
  __ mov(StoreWithVectorDescriptor::SlotRegister(), Operand(Smi::FromInt(bytecode_iterator().GetIndexOperand(2))));
  LoadFeedbackVector(StoreWithVectorDescriptor::VectorRegister());
  __ mov(StoreWithVectorDescriptor::ValueRegister(), kInterpreterAccumulatorRegister);
  
  __ Call(ic.code());
}

void FastCodeGenerator::VisitStaNamedPropertySloppy() {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY);
  DoStoreIC(ic);
}

void FastCodeGenerator::VisitStaNamedPropertyStrict() {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, STRICT);
  DoStoreIC(ic);
}

void FastCodeGenerator::VisitStaNamedOwnProperty() {
  Callable ic = CodeFactory::StoreOwnICInOptimizedCode(isolate_);
  DoStoreIC(ic);
}

void FastCodeGenerator::DoKeyedStoreIC(Callable ic) {
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), StoreWithVectorDescriptor::ReceiverRegister());
  LoadRegister(bytecode_iterator().GetRegisterOperand(1), StoreWithVectorDescriptor::NameRegister());
  __ mov(StoreWithVectorDescriptor::ValueRegister(), kInterpreterAccumulatorRegister);
  __ mov(StoreWithVectorDescriptor::SlotRegister(), Operand(Smi::FromInt(bytecode_iterator().GetIndexOperand(2))));
  LoadFeedbackVector(StoreWithVectorDescriptor::VectorRegister());
  __ Call(ic.code());
}

void FastCodeGenerator::VisitStaKeyedPropertySloppy() {
  Callable ic = CodeFactory::KeyedStoreICInOptimizedCode(isolate_, SLOPPY);
  DoKeyedStoreIC(ic);
}

void FastCodeGenerator::VisitStaKeyedPropertyStrict() {
  Callable ic = CodeFactory::KeyedStoreICInOptimizedCode(isolate_, STRICT);
  DoKeyedStoreIC(ic);
}

void FastCodeGenerator::VisitLdaModuleVariable() {
  __builtin_unreachable();
}

void FastCodeGenerator::VisitStaModuleVariable() {
  __builtin_unreachable();
}

void FastCodeGenerator::VisitPushContext() {
  StoreRegister(bytecode_iterator().GetRegisterOperand(0), cp);
  SetContext(kInterpreterAccumulatorRegister);
}

void FastCodeGenerator::VisitPopContext() {
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), cp);
}

void FastCodeGenerator::VisitCreateClosure() {
  Handle<Object> shared = bytecode_iterator().GetConstantForIndexOperand(0);
  uint32_t flags = bytecode_iterator().GetFlagOperand(2);
  if (CreateClosureFlags::FastNewClosureBit::decode(flags)) {
    __ mov(r1, Operand(shared));
    LoadFeedbackVector(r2);
    __ mov(r3, Operand(Smi::FromInt(bytecode_iterator().GetIndexOperand(1))));
    Callable callable = CodeFactory::FastNewClosure(isolate_);
    __ Call(callable.code());
  } else {
    int tenured_raw = CreateClosureFlags::PretenuredBit::decode(flags);
    __ mov(r1, Operand(shared));
    __ push(r1);
    LoadFeedbackVector(r1);
    __ push(r1);
    uint32_t vector_index = bytecode_iterator().GetIndexOperand(1);
    __ mov(r1, Operand(vector_index));
    __ push(r1);
    __ mov(r1, Operand(Smi::FromInt(tenured_raw)));
    __ push(r1);
    __ CallRuntime(Runtime::kInterpreterNewClosure);
  }
  
}

void FastCodeGenerator::VisitCreateBlockContext() {
  Handle<Object> scope_info = bytecode_iterator.GetConstantForIndexOperand(0);
  __ mov(r1, Operand(scope_info));
  __ push(r1);
  __ CallRuntime(Runtime::kPushBlockContext);
}

void FastCodeGenerator::VisitCreateFunctionContext() {
  LoadRegister(interpreter::Register::function_closure(), FastNewFunctionContextDescriptor::FunctionRegister());
  __ mov(FastNewFunctionContextDescriptor::SlotsRegister(), Operand(bytecode_iterator().GetUnsignedImmediateOperand(0)));
  Callable callable = CodeFactory::FastNewFunctionContext(FUNCTION_SCOPE);
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCreateEvalContext() {
  LoadRegister(interpreter::Register::function_closure(), FastNewFunctionContextDescriptor::FunctionRegister());
  __ mov(FastNewFunctionContextDescriptor::SlotsRegister(), Operand(bytecode_iterator().GetUnsignedImmediateOperand(0)));
  Callable callable = CodeFactory::FastNewFunctionContext(EVAL_SCOPE);
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCreateCatchContext() {
  Handle<Object> name = bytecode_iterator().GetConstantForIndexOperand(1);
  __ mov(r1, Operand(name));
  __ push(r1);
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), r1);
  __ push(r1);
  Handle<Object> scope_info = bytecode_iterator().GetConstantForIndexOperand(2);
  __ mov(r1, Operand(scope_info));
  __ push(r1);
  __ push(kInterpreterAccumulatorRegister);
  __ CallRuntime(Runtime::kPushCatchContext);
}

void FastCodeGenerator::VisitCreateWithContext() {
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), r1);
  __ push(r1);
  Handle<Object> scope_info = bytecode_iterator().GetConstantForIndexOperand(1);
  __ mov(r1, Operand(scope_info));
  __ push(r1);
  __ push(kInterpreterAccumulatorRegister);
  __ CallRuntime(Runtime::kPushWithContext);
}

void FastCodeGenerator::VisitCreateMappedArguments() {
  Label if_duplicate_parameters, done;
  LoadRegister(interpreter::Register::function_closure(), r1);
  __ ldr(r1, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r1, FieldMemOperand(r1, SharedFunctionInfo::kHasDuplicateParametersByteOffset));
  __ tst(r1, Operand(1 << SharedFunctionInfo::kHasDuplicateParametersBitWithinByte));
  __ b(&if_duplicate_parameters, ne);
  Callable callable = CodeFactory::FastNewSloppyArguments(isolate_);
  
  LoadRegister(interpreter::Register::function_closure(), FastNewArgumentsDescriptor::TargetRegister());
  __ Call(callable.code());
  __ b(&done);
  __ bind(&if_duplicate_parameters);
  LoadRegister(interpreter::Register::function_closure(), r1);
  __ push(r1);
  __ CallRuntime(Runtime::kNewSloppyArguments_Generic);
  __ bind(&done);
}

void FastCodeGenerator::VisitCreateUnmappedArguments() {
  Callable callable = CodeFactory::FastNewStrictArguments(isolate_);
  
  LoadRegister(interpreter::Register::function_closure(), FastNewArgumentsDescriptor::TargetRegister());
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCreateRestParameter() {
  Callable callable = CodeFactory::FastNewRestParameter(isolate_);
  
  LoadRegister(interpreter::Register::function_closure(), FastNewArgumentsDescriptor::TargetRegister());
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCreateRegExpLiteral() {
  Handle<Object> pattern = bytecode_iterator().GetConstantForIndexOperand(0);
  uint32_t literal_index = bytecode_iterator().GetIndexOperand(1);
  uint32_t flags = bytecode_iterator().GetFlagOperand(2);
  Callable callable = CodeFactory::FastCloneRegExp(isolate_);
  LoadRegister(interpreter::Register::function_closure(), r3);
  __ mov(r2, Operand(Smi::FromInt(literal_index)));
  __ mov(r1, Operand(pattern));
  __ mov(r0, Operand(Smi::FromInt(flags)));
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCreateArrayLiteral() {
  uint32_t bytecode_flags = bytecode_iterator().GetFlagOperand(2);
  Handle<Object> constant_elements = bytecode_iterator().GetConstantForIndexOperand(0);
  if (CreateArrayLiteralFlags::FastShallowCloneBit::decode(bytecode_flags)) {
    Callable callable = CodeFactory::FastCloneShallowArray(isolate_, TRACK_ALLOCATION_SITE);
    LoadRegister(interpreter::Register::function_closure(), r3);
    uint32_t literal_index = bytecode_iterator().GetIndexOperand(1);
    __ mov(r2, Operand(Smi::FromInt(literal_index)));
    __ mov(r1, Operand(constant_elements));
    __ Call(callable.code());
  } else {
    uint32_t flags = CreateArrayLiteralFlags::FlagsBits::decode(bytecode_flags);
    LoadRegister(interpreter::Register::function_closure(), r1);
    __ push(r1);
    uint32_t literal_index = bytecode_iterator().GetIndexOperand(1);
    __ mov(r1, Operand(Smi::FromInt(literal_index)));
    __ push(r1);
    __ mov(r1, Operand(constant_elements));
    __ push(r1);
    __ mov(r1, Operand(Smi::FromInt(flags)));
    __ push(r1);
    __ CallRuntime(Runtime::kCreateArrayLiteral);
  }
}

void FastCodeGenerator::VisitCreateObjectLiteral() {
  uint32_t bytecode_flags = bytecode_iterator().GetFlagOperand(2);
  uint32_t literal_index = bytecode_iterator().GetIndexOperand(1);
  uint32_t fast_clone_properties_count = CreateObjectLiteralFlags::FastClonePropertiesCountBits::decode(bytecode_flags);
  uint32_t flags = CreateObjectLiteralFlags::FlagsBits::decode(bytecode_flags);
  Handle<Object> constant_elements = bytecode_flags.GetConstantForIndexOperand(0);
  if (fast_clone_properties_count) {
    LoadRegister(interpreter::Register::function_closure(), r3);
    __ mov(r2, Operand(Smi::FromInt(literal_index)));
    __ mov(r1, Operand(constant_elements));
    __ push(r0);
    __ mov(r0, Operand(flags));
    Callable callable = CodeFactory::FastCloneShallowObject(isolate_, fast_clone_properties_count);
    __ Call(callable.code());
    StoreRegister(bytecode_iterator().GetRegisterOperand(3), r0);
    __ pop(r0);
  } else {
    LoadRegister(interpreter::Register::function_closure(), r1);
    __ push(r0);
    __ push(r1);
    __ mov(r1, Operand(Smi::FromInt(literal_index)));
    __ push(r1);
    __ mov(r1, Operand(constant_elements));
    __ push(r1);
    __ mov(r1, Operand(flags));
    __ push(r1);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
    StoreRegister(bytecode_iterator().GetRegisterOperand(3), r0);
    __ pop(r0);

  }
}

void FastCodeGenerator::DoJSCall(TailCallMode tail_call_mode) {
  Callable callable = CodeFactory::InterpreterPushArgsAndCall(isolate_, tail_call_mode, InterpreterPushArgsMode::kOther);
  LoadRegister(bytecode_iterator().GetRegisterOperand(0), r1);
  __ add(r2, fp, Operand(bytecode_iterator().GetRegisterOperand(1).ToOperand() << kPointerSize));
  uint32_t receiver_args_count = bytecode_iterator().GetUnsignedImmediateOperand(2);
  receiver_args_count -= 1;
  __ mov(r0, Operand(receiver_args_count));
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCall() {
  DoJSCall(TailCallMode::kDisallow);
}

void FastCodeGenerator::VisitCallWithSpread() {
  DoJSCall(TailCallMode::kDisallow);
}

void FastCodeGenerator::VisitCallProperty() {
  DoJSCall(assembler, TailCallMode::kDisallow);
}

void FastCodeGenerator::VisitTailCall() {
  DoJSCall(TailCallMode::kAllow);
}

void FastCodeGenerator::VisitCallJSRuntime() {
  Callable callable = CodeFactory::InterpreterPushArgsAndCall(isolate_, tail_call_mode, InterpreterPushArgsMode::kOther);
  uint32_t context_index = bytecode_iterator().GetIndexOperand(0);
  __ ldr(r1, ContextMemOperand(cp, Context::NATIVE_CONTEXT_INDEX));
  __ ldr(r1, ContextMemOperand(r1, context_index));
  __ add(r2, fp, Operand(bytecode_iterator().GetRegisterOperand(1).ToOperand() << kPointerSize));
  uint32_t receiver_args_count = bytecode_iterator().GetUnsignedImmediateOperand(2);
  receiver_args_count -= 1;
  __ mov(r0, Operand(receiver_args_count));
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCallRuntime() {
  uint32_t function_id = bytecode_iterator().GetUnsignedImmediateOperand(0);
  const Runtime::Function* function = Runtime::FunctionForId(static_cast<Runtime::FunctionId>(function_id));
  ExternalReference runtime_function(static_cast<Runtime::FunctionId>(function_id), isolate_);
  uint32_t args_count = bytecode_iterator().GetUnsignedImmediateOperand(2);
  __ mov(r0, Operand(args_count));
  __ add(r2, fp, Operand(bytecode_iterator().GetRegisterOperand(1).ToOperand() << kPointerSize));
  __ mov(r1, Operand(runtime_function));
  Callable callable = CodeFactory::InterpreterCEntry(isolate(), 1);
  __ Call(callable.code());
}

void FastCodeGenerator::VisitCallRuntimeForPair() {
  uint32_t function_id = bytecode_iterator().GetUnsignedImmediateOperand(0);
  const Runtime::Function* function = Runtime::FunctionForId(static_cast<Runtime::FunctionId>(function_id));
  ExternalReference runtime_function(static_cast<Runtime::FunctionId>(function_id), isolate_);
  uint32_t args_count = bytecode_iterator().GetUnsignedImmediateOperand(2);
  __ mov(r0, Operand(args_count));
  __ add(r2, fp, Operand(bytecode_iterator().GetRegisterOperand(1).ToOperand() << kPointerSize));
  __ mov(r1, Operand(runtime_function));
  Callable callable = CodeFactory::InterpreterCEntry(isolate(), 2);
  __ Call(callable.code());
}

void FastCodeGenerator::VisitConstructWithSpread() {
}

void FastCodeGenerator::VisitInvokeIntrinsic() {
}

void FastCodeGenerator::VisitConstruct() {
}

void FastCodeGenerator::VisitThrow() {
}

void FastCodeGenerator::VisitReThrow() {
}

void FastCodeGenerator::VisitAdd() {
}

void FastCodeGenerator::VisitSub() {
}

void FastCodeGenerator::VisitMul() {
}

void FastCodeGenerator::VisitDiv() {
}

void FastCodeGenerator::VisitMod() {
}

void FastCodeGenerator::VisitBitwiseOr() {
}

void FastCodeGenerator::VisitBitwiseXor() {
}

void FastCodeGenerator::VisitBitwiseAnd() {
}

void FastCodeGenerator::VisitShiftLeft() {
}

void FastCodeGenerator::VisitShiftRight() {
}

void FastCodeGenerator::VisitShiftRightLogical() {
}

void FastCodeGenerator::VisitAddSmi() {
}

void FastCodeGenerator::VisitSubSmi() {
}

void FastCodeGenerator::VisitBitwiseOrSmi() {
}

void FastCodeGenerator::VisitBitwiseAndSmi() {
}

void FastCodeGenerator::VisitShiftLeftSmi() {
}

void FastCodeGenerator::VisitShiftRightSmi() {
}

void FastCodeGenerator::VisitInc() {
}

void FastCodeGenerator::VisitDec() {
}

void FastCodeGenerator::VisitLogicalNot() {
}

void FastCodeGenerator::VisitToBooleanLogicalNot() {
}

void FastCodeGenerator::VisitTypeOf() {
}

void FastCodeGenerator::VisitDeletePropertyStrict() {
}

void FastCodeGenerator::VisitDeletePropertySloppy() {
}

void FastCodeGenerator::VisitGetSuperConstructor() {
}

void FastCodeGenerator::VisitTestEqual() {
}

void FastCodeGenerator::VisitTestNotEqual() {
}

void FastCodeGenerator::VisitTestEqualStrict() {
}

void FastCodeGenerator::VisitTestLessThan() {
}

void FastCodeGenerator::VisitTestGreaterThan() {
}

void FastCodeGenerator::VisitTestLessThanOrEqual() {
}

void FastCodeGenerator::VisitTestGreaterThanOrEqual() {
}

void FastCodeGenerator::VisitTestIn() {
}

void FastCodeGenerator::VisitTestInstanceOf() {
}

void FastCodeGenerator::VisitTestUndetectable() {
}

void FastCodeGenerator::VisitTestNull() {
}

void FastCodeGenerator::VisitTestUndefined() {
}

void FastCodeGenerator::VisitToName() {
}

void FastCodeGenerator::VisitToObject() {
}

void FastCodeGenerator::VisitToNumber() {
}

void FastCodeGenerator::VisitJump() { 
}

void FastCodeGenerator::VisitJumpConstant() {  }

void FastCodeGenerator::VisitJumpIfTrue() {  }

void FastCodeGenerator::VisitJumpIfTrueConstant() {  }

void FastCodeGenerator::VisitJumpIfFalse() {  }

void FastCodeGenerator::VisitJumpIfFalseConstant() {  }

void FastCodeGenerator::VisitJumpIfToBooleanTrue() {
}

void FastCodeGenerator::VisitJumpIfToBooleanTrueConstant() {
}

void FastCodeGenerator::VisitJumpIfToBooleanFalse() {
}

void FastCodeGenerator::VisitJumpIfToBooleanFalseConstant() {
}

void FastCodeGenerator::VisitJumpIfNotHole() {
}

void FastCodeGenerator::VisitJumpIfNotHoleConstant() {
}

void FastCodeGenerator::VisitJumpIfJSReceiver() {
}

void FastCodeGenerator::VisitJumpIfJSReceiverConstant() {
}

void FastCodeGenerator::VisitJumpIfNull() {
}

void FastCodeGenerator::VisitJumpIfNullConstant() {
}

void FastCodeGenerator::VisitJumpIfUndefined() {
}

void FastCodeGenerator::VisitJumpIfUndefinedConstant() {
}

void FastCodeGenerator::VisitJumpLoop() { 
}

void FastCodeGenerator::VisitStackCheck() {
}

void FastCodeGenerator::VisitSetPendingMessage() {
}

void FastCodeGenerator::VisitReturn() {
}

void FastCodeGenerator::VisitDebugger() {
}

void FastCodeGenerator::VisitForInPrepare() {
}

void FastCodeGenerator::VisitForInContinue() {
}

void FastCodeGenerator::VisitForInNext() { 
}

void FastCodeGenerator::VisitForInStep() {
}

void FastCodeGenerator::VisitSuspendGenerator() {
}

void FastCodeGenerator::VisitResumeGenerator() {
}

void FastCodeGenerator::VisitWide() {
}

void FastCodeGenerator::VisitExtraWide() {
}

void FastCodeGenerator::VisitIllegal() {
}

void FastCodeGenerator::VisitNop() {
}
}
}
