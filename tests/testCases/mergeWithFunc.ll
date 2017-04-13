; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i32 @foo(i32 %i, i32 %j, i32 %k) {
entry:
  %mul = mul nsw i32 %k, %j
  %mul1 = mul nsw i32 %i, 5
  %add = add nuw nsw i32 %mul1, %mul
  %sub = sub nsw i32 %mul1, %mul
  %mul4 = mul nsw i32 %add, %sub
  ret i32 %mul4
}

; CHECK-LABEL: @bar
define i32 @bar(i32 %i, i32 %j, i32 %k) {
entry:
  %cmp = icmp slt i32 %i, 0
  br i1 %cmp, label %if.then, label %return

if.then:
  ; CHECK: call i32 @foo(i32 %j, i32 %i, i32 %k)
  %mul = mul nsw i32 %k, %i
  %mul1 = mul nsw i32 %j, 5
  %add = add nuw nsw i32 %mul1, %mul
  %sub = sub nsw i32 %mul1, %mul
  %mul4 = mul nsw i32 %add, %sub
  br label %return

return:
  %retval.0 = phi i32 [ %mul4, %if.then ], [ 7, %entry ]
  ret i32 %retval.0
}

define i32 @main() {
  %call1 = call i32 @foo(i32 3, i32 4, i32 5)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar(i32 5, i32 4, i32 3)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  ret i32 0
}

declare i32 @printf(i8*, ...)
