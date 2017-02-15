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

define i32 @almostFoo(i32 %k) {
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

