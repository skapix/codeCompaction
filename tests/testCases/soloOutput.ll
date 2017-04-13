; check, that it optimizes without errors
; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i32 @foo(i32 %i) {
entry:
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  br  label %end
end:
  ret i32 %someCalc5
}

define i32 @bar(i32 %i) {
entry:
  %cmp = icmp slt i32 %i, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  ret i32 %someCalc5

cleanup:
  ret i32 0
}

define i32 @main() {
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar(i32 4)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  ret i32 0
}

declare i32 @printf(i8*, ...)
