; check, that it optimizes without errors
; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; Also test FunctionCompiler
; RUN: opt -S -load  %opt_path %pass_name < %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
; CHECK-LABEL: foo0
define i32 @foo0(i32 %i) {
entry:
; CHECK: call{{[a-z ]*}} i32 [[FName0:@[_\.A-Za-z0-9]+]]
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  br label %br_label
br_label:
  %comp = icmp slt i32 %someCalc5, %someCalc4
  br i1 %comp, label %end, label %end2
end:
  ret i32 %someCalc5
end2:
  ret i32 %someCalc4
}

; CHECK-LABEL: bar0
define i32 @bar0(i32 %i, i32 %j) {
entry:
; CHECK: call{{[a-z ]*}} i32 [[FName0]]
  %calc1 = mul nsw i32 %i, %i
  %calc2 = mul nsw i32 %i, %calc1
  %calc3 = add nsw i32 %calc2, %calc1
  %calc4 = sub nsw i32 %calc3, %calc1
  %calc5 = mul nsw i32 %calc3, %calc4
  br label %end
end:
  %res = add nsw i32 %j, %calc5
  ret i32 %res
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

define i32 @min(i32 %i0, i32 %i1) {
entry:
  %cmp = icmp ult i32 %i0, %i1
  br i1 %cmp, label %end1, label %end2
end1:
  ret i32 %i0
end2:
  ret i32 %i1
}

; CHECK-LABEL: foo1
define i32 @foo1(i32 %i) {
entry:
; CHECK: call{{[a-z ]*}} i32 [[FName1:@[_\.A-Za-z0-9]+]]
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = sub nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = call i32 @min(i32 %someCalc4, i32 %someCalc3)
  br label %br_label
br_label:
  %comp = icmp slt i32 %someCalc5, %someCalc4
  br i1 %comp, label %end, label %end2
end:
  ret i32 %someCalc5
end2:
  ret i32 %someCalc4
}

; CHECK-LABEL: bar1
define i32 @bar1(i32 %i, i32 %j) {
cont:
; CHECK: call{{[a-z ]*}} i32 [[FName1]]
  %calc1 = mul nsw i32 %i, %i
  %calc2 = sub nsw i32 %i, %calc1
  %calc3 = add nsw i32 %calc2, %calc1
  %calc4 = sub nsw i32 %calc3, %calc1
  %calc5 = call i32 @min(i32 %calc4, i32 %calc3)
  br label %end
end:
  %res = add nsw i32 %j, %calc5
  ret i32 %res
}

; CHECK-LABEL: @main
define i32 @main() {
  %call1 = call i32 @foo0(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar0(i32 3, i32 2)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  %call5 = call i32 @foo0(i32 4)
  %call6 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call5)
  %call7 = call i32 @bar0(i32 5, i32 6)
  %call8 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call7)
  ret i32 0
}

declare i32 @printf(i8*, ...)
