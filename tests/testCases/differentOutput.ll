define i32 @"\01?foo@@YAHH@Z"(i32 %k) {
entry:
  %call = tail call i32 @"\01?someCalc@@YAHH@Z"(i32 %k)
  %conv = trunc i32 %call to i8
  %call1 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %k, i8 %conv)
  %conv2 = trunc i32 %call1 to i8
  %call3 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %call, i8 %conv2)
  %cmp = icmp eq i32 %call, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %cmp4 = icmp eq i32 %call3, 2
  %mul = mul nsw i32 %k, 3
  %mul.call3 = select i1 %cmp4, i32 %mul, i32 %call3
  ret i32 %mul.call3

cleanup:                                          ; preds = %entry
  ret i32 %k
}

declare i32 @"\01?someCalc@@YAHH@Z"(i32)

declare i32 @"\01?someCalc@@YAHHD@Z"(i32, i8)

define i32 @"\01?almostFoo@@YAHH@Z"(i32 %k) local_unnamed_addr #0 {
entry:
  %call = tail call i32 @"\01?someCalc@@YAHH@Z"(i32 %k)
  %conv = trunc i32 %call to i8
  %call1 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %k, i8 %conv)
  %conv2 = trunc i32 %call1 to i8
  %call3 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %call, i8 %conv2)
  %cmp = icmp eq i32 %call, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %cmp4 = icmp eq i32 %call3, 2
  %mul = mul nsw i32 %k, 3
  %mul.call = select i1 %cmp4, i32 %mul, i32 %call
  ret i32 %mul.call

cleanup:                                          ; preds = %entry
  ret i32 %call1
}

