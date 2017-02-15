@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i32 @foo(i32 %k) {
entry:
  %someCalc1 = add nsw i32 %k, 31
  %someCalc2 = mul nsw i32 %someCalc1, 5
  %someCalc3 = add nsw i32 %someCalc1, %someCalc2
  %someCalc4 = add nsw i32 %someCalc3, %someCalc3
  %cmp = icmp eq i32 %someCalc4, 10
  br i1 %cmp, label %cleanup, label %if.end

if.end:
  %cmp4 = icmp eq i32 %someCalc3, 2
  %mul = mul nsw i32 %k, 3
  %res = select i1 %cmp4, i32 %mul, i32 %someCalc3
  ret i32 %res

cleanup:
  ret i32 %k
}

define i32 @bar(i32 %k) {
entry:
  %someCalc1 = add nsw i32 %k, 31
  %someCalc2 = mul nsw i32 %someCalc1, 5
  %someCalc3 = add nsw i32 %someCalc1, %someCalc2
  %someCalc4 = add nsw i32 %someCalc3, %someCalc3
  %cmp = icmp eq i32 %someCalc4, 10
  br i1 %cmp, label %cleanup, label %if.end

if.end:
  %cmp4 = icmp eq i32 %someCalc3, 3
  %mul = mul nsw i32 %k, 10
  %res = select i1 %cmp4, i32 %mul, i32 %someCalc3
  ret i32 %res

cleanup:
  ret i32 %someCalc2
}

define i32 @main() {
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar(i32 4)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  ret i32 0
}

declare i32 @printf(i8*, ...)
