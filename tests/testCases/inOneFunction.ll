; check, that it optimizes without errors
; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; Also test FunctionCompiler
; RUN: opt -S -load  %opt_path %pass_name < %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; CHECK-LABEL: @foo
define i32 @foo(i32 %i) {
entry:
; CHECK: entry
; CHECK: call{{[a-z ]*}} i32 [[FName:@[_\.A-Za-z0-9]+]]
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  br  label %br_label

br_label:
  %comp = icmp slt i32 %someCalc5, 4
  br i1 %comp, label %cont, label %end

cont:
; CHECK: cont
; CHECK: call{{[a-z ]*}} i32 [[FName]]
  %calc1 = mul nsw i32 %someCalc1, %someCalc1
  %calc2 = mul nsw i32 %someCalc1, %calc1
  %calc3 = add nsw i32 %calc2, %calc1
  %calc4 = sub nsw i32 %calc3, %calc1
  %calc5 = mul nsw i32 %calc3, %calc4
  ret i32 %calc1

end:
  ret i32 %someCalc5
}

; CHECK-LABEL: @main
define i32 @main() {
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  ret i32 0
}

declare i32 @printf(i8*, ...)

; CHECK: i32 [[FName]]
