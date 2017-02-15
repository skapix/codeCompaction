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
entry:
  ret i32 0
}