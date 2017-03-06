; RUN: opt -S -load  %opt_path -bbfactor -bbfactor-force-merging < %s | FileCheck %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; CHECK-LABEL: @foo
define void @foo(i32 %i) {
entry:
; CHECK: %someCalc1 = mul nsw i32 %i, %i
; CHECK-NEXT: %someCalc2 = mul nsw i32 %i, %someCalc1
; CHECK-NEXT: %someCalc3 = add nsw i32 %someCalc2, %someCalc1
; CHECK-NOT call void
; CHECK: ret void
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  %call1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %someCalc5)
  ret void
}

; CHECK-LABEL: define void @bar
define void @bar(i32 %i) {
entry:
  %cmp = icmp sgt i32 %i, 1
  br i1 %cmp, label %if.then, label %if.else
if.then:
 ; CHECK-NOT: mul nsw i32 %i, %i
; CHECK: call void @foo(i32 %i)
; CHECK-NOT: mul nsw i32 %i, %i
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  %call1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %someCalc5)
  br label %if.else
if.else:
  ret void
}

; CHECK: define i32 @main
define i32 @main() {
entry:
  call void @foo(i32 3)
  call void @bar(i32 5)
  ret i32 0
}

; CHECK: declare i32 @printf
declare i32 @printf(i8*, ...)

; check, that new function was not created
; CHECK-NOT: define
