; RUN: opt -S -load  %opt_path -bbfactor -bbfactor-force-merging < %s | FileCheck %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; CHECK-LABEL: define i32 @foo
define i32 @foo(i32 %i) {
entry:
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  %call1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %someCalc5)
  ret i32 0
}

; CHECK-LABEL: define void @bar
define void @bar(i32 %i) {
entry:
  %cmp = icmp sgt i32 %i, 1
  br i1 %cmp, label %if.then, label %if.else
if.then:
; CHECK: call i32 @foo(i32 %i)
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
  %call1 = call i32 @foo(i32 3)
  call void @bar(i32 5)
  ret i32 0
}

; CHECK: declare i32 @printf
declare i32 @printf(i8*, ...)
; CHECK-NOT: {{declare|define}}