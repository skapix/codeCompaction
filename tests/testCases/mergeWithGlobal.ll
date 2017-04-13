; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@g_globalValue = local_unnamed_addr global i32 0, align 4

define i32 @foo(i32 %i) {
entry:
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  %0 = load i32, i32* @g_globalValue, align 4
  %inc = add nsw i32 %0, %someCalc5
  store i32 %inc, i32* @g_globalValue, align 4
  ret i32 %inc
}


; CHECK-LABEL: @bar
define i32 @bar(i32 %i) {
entry:
  %cmp = icmp slt i32 %i, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %0 = load i32, i32* @g_globalValue, align 4
  %inc0 = add nsw i32 %0, 1
  store i32 %inc0, i32* @g_globalValue, align 4
  br label %if.end

if.end: 
; CHECK: call i32 @foo(i32 %i)
  %someCalc1 = mul nsw i32 %i, %i
  %someCalc2 = mul nsw i32 %i, %someCalc1
  %someCalc3 = add nsw i32 %someCalc2, %someCalc1
  %someCalc4 = sub nsw i32 %someCalc3, %someCalc1
  %someCalc5 = mul nsw i32 %someCalc3, %someCalc4
  %1 = load i32, i32* @g_globalValue, align 4
  %inc = add nsw i32 %1, %someCalc5
  store i32 %inc, i32* @g_globalValue, align 4
  ret i32 %inc
}

define i32 @main() {
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar(i32 4)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  ret i32 0
}

declare i32 @printf(i8*, ...)
