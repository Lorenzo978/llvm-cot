; RUN: opt -passes=licm -S < %s | FileCheck %s

; Some random methods that will be used inside the loops. What's key is whether
; these can throw an exception.
declare void @use(i64 %a)
declare void @may_throw()
declare void @will_not_throw() nounwind

; NOTE: I used {{^}} in front of the labels representing basic block to avoid
; these being matched with the comments generated by opt (or branch instruction
; arguments)

;------------------------------------------------------------------------------
; CASE 1: A loop without functions calls - hoisting successful
;------------------------------------------------------------------------------
define void @trivially_hoistable(i32 %input) {
; CHECK-LABEL: trivially_hoistable
; CHECK-LABEL: {{^}}entry
; CHECK:  %temp = mul i32 %input, 2
; CHECK-LABEL: {{^}}header_block
; CHECK-LABEL: {{^}}latch_block
; CHECK-LABEL: {{^}}exit
entry:
  br label %header_block

header_block:
  %j = phi i32 [ 0, %entry ], [ %next, %latch_block ]
  br label %latch_block

latch_block:
  %temp = mul i32 %input, 2
  %next = add i32 %j, %temp
  %cond = icmp slt i32 %next, 10
  br i1 %cond, label %exit, label %header_block

exit:
  ret void
}

;------------------------------------------------------------------------------
; CASE 2: A loop with a function call - hoisting successful
;------------------------------------------------------------------------------
define void @call_after_hoistable_inst(i64 %x, i64 %y, i1 %cond) {
; CHECK-LABEL: call_after_hoistable_inst
; CHECK-LABEL: {{^}}entry
; CHECK: %div = udiv i64 %x, %y
; CHECK-LABEL: {{^}}header_block
; CHECK-LABEL: {{^}}latch_block
; CHECK: call void @use(i64 %div)
entry:
  br label %header_block

header_block:
  %div = udiv i64 %x, %y
  br label %latch_block

latch_block:
  call void @use(i64 %div)
  br label %header_block
}

;------------------------------------------------------------------------------
; CASE 3: A loop with a function call - hoisting unsuccessful
;------------------------------------------------------------------------------
define void @call_before_hoistable_inst(i64 %x, i64 %y, i1 %cond) {
; CHECK-LABEL: call_before_hoistable_inst
; CHECK-LABEL: {{^}}entry
; CHECK-LABEL: {{^}}header_block
; CHECK: %div = udiv i64 %x, %y
; CHECK: br label %latch_block
; CHECK-LABEL: {{^}}latch_block
; CHECK: call void @use(i64 %div)
entry:
  br label %header_block

header_block:
  call void @may_throw()
  %div = udiv i64 %x, %y
  br label %latch_block

latch_block:
  call void @use(i64 %div)
  br label %header_block
}

;------------------------------------------------------------------------------
; CASE 3: A loop with a function call - hoisting successful
;------------------------------------------------------------------------------
define void @call_before_hoistable_inst_v2(i64 %x, i64 %y, i1 %cond) {
; CHECK-LABEL: call_before_hoistable_inst_v2
; CHECK-LABEL: {{^}}entry
; CHECK-LABEL: {{^}}header_block:
; CHECK: %div = udiv i64 %x, %y
; CHECK-LABEL: {{^}}latch_block:
; CHECK: call void @use(i64 %div)
entry:
  br label %header_block

header_block:
  call void @will_not_throw()
  %div = udiv i64 %x, %y
  br label %latch_block

latch_block:
  call void @use(i64 %div)
  br label %header_block
}

;------------------------------------------------------------------------------
; CASE 4: A loop with load instructions & a function call - hoisting succesful
;------------------------------------------------------------------------------
define void @throw_header_after_rec(ptr %xp, ptr %yp, i1 %cond) {
; CHECK-LABEL: throw_header_after_rec
; CHECK-LABEL: {{^}}entry
; CHECK: %x = load i64, ptr %xp
; CHECK: %y = load i64, ptr %yp
; CHECK: %div = udiv i64 %x, %y
; CHECK-LABEL: {{^}}header_block
; CHECK-LABEL: {{^}}latch_block
; CHECK: call void @use(i64 %div)
entry:
  br label %header_block

header_block:
  %x = load i64, ptr %xp
  %y = load i64, ptr %yp
  %div = udiv i64 %x, %y
  br label %latch_block

latch_block:
  call void @use(i64 %div) readonly
  br label %header_block
}
