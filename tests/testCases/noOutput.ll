; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; CHECK-LABEL: @foo
define i32 @foo(i32 %i) {
entry:
  %cmp = icmp sge i32 %i, 0
  br i1 %cmp, label %if.then, label %if.else
if.then:
; CHECK: call{{[a-z ]*}} void [[FName:@[_\.a-z0-9]+]](i32 %i)
; CHECK-NEXT: ret i32 0
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  %call1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %someCalc5)
  ret i32 0
if.else:
  ret i32 %i
}

; CHECK-LABEL: @bar
define i32 @bar(i32 %i) {
entry:
  %cmp = icmp sgt i32 %i, 1
  br i1 %cmp, label %if.then, label %if.else
if.then:
; CHECK: call{{[a-z ]*}} void [[FName]](i32 %i)
; CHECK-NEXT: ret i32 1
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  %call1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %someCalc5)
  ret i32 1


if.else:
  ret i32 %i
}

define i32 @main() {
entry:
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 @bar(i32 5)
  ret i32 0
}

declare i32 @printf(i8*, ...)

; CHECK: define private {{[a-z]*}} void [[FName]](i32{{[0-9a-z]*}})
; CHECK-NEXT: Entry:
; CHECK-NEXT: [[C1:%[_\.a-z0-9]+]] = mul nsw i32 [[C0:%[_\.a-z0-9]+]], [[C0]]
; CHECK-NEXT: [[C2:%[_\.a-z0-9]+]] = mul nsw i32 [[C0]], [[C1]]
; CHECK-NEXT: [[C3:%[_\.a-z0-9]+]] = add nsw i32 [[C2]], [[C1]]
; CHECK-NEXT: [[C4:%[_\.a-z0-9]+]] = sub nsw i32 [[C3]], [[C1]]
; CHECK-NEXT: [[C5:%[_\.a-z0-9]+]] = mul nsw i32 [[C3]], [[C4]]
; CHECK-NEXT: %{{[_\.a-z0-9]+}} = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 [[C5]])
; CHECK-NEXT: ret void
