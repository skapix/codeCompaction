; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; CHECK-LABEL: @foo
define i32 @foo(i32 %k) {
entry:
; CHECK: [[P0:%[_\.a-z0-9]+]] = alloca i32
; CHECK-NEXT: call{{[a-z ]*}} i32  [[FName:@[_\.a-z0-9]+]](i32 %k, i32* [[P0]])
; CHECK-NEXT: load i32, i32* [[P0]]
  %someCalc1 = add nsw i32 %k, 31
  %someCalc2 = mul nsw i32 %someCalc1, 5
  %someCalc3 = add nsw i32 %someCalc1, %someCalc2
  %someCalc4 = add nsw i32 %someCalc3, %someCalc3
  br label %if.end
if.end:
  %res = mul nsw i32 %someCalc3, %someCalc4
  ret i32 %res
}

  ; CHECK-LABEL: @bar
define i32 @bar(i32 %k) {
entry:
; CHECK: [[P10:%[_\.a-z0-9]+]] = alloca i32
; CHECK-NEXT: call{{[a-z ]*}} i32 [[FName]](i32 %k, i32* [[P10]])
  %someCalc1 = add nsw i32 %k, 31
  %someCalc2 = mul nsw i32 %someCalc1, 5
  %someCalc3 = add nsw i32 %someCalc1, %someCalc2
  %someCalc4 = add nsw i32 %someCalc3, %someCalc3
  br label %if.end


if.end:
  %res = mul nsw i32 3, %someCalc4
  ret i32 %res
}

define i32 @main() {
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar(i32 4)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  ret i32 0
}

declare i32 @printf(i8*, ...)

; CHECK: define private {{[a-z]*}} i32 [[FName]](i32{{[0-9a-z]*}}, i32*
