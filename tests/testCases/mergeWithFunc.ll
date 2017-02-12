define i32 @foo(i32 %i, i32 %j) {
entry:
  %mul = mul nsw i32 %i, %i
  %mul1 = mul nsw i32 %j, %j
  %add = add nuw nsw i32 %mul1, %mul
  %sub = sub nsw i32 %mul, %mul1
  %mul4 = mul nsw i32 %add, %sub
  ret i32 %mul4
}

define i32 @bar(i32 %i, i32 %j, i32 %k) {
entry:
  %cmp = icmp slt i32 %k, 0
  br i1 %cmp, label %if.then, label %return

if.then:                                          ; preds = %entry
  %mul = mul nsw i32 %i, %i
  %mul1 = mul nsw i32 %j, %j
  %add = add nuw nsw i32 %mul1, %mul
  %sub = sub nsw i32 %mul, %mul1
  %mul4 = mul nsw i32 %add, %sub
  br label %return

return:                                           ; preds = %entry, %if.then
  %retval.0 = phi i32 [ %mul4, %if.then ], [ %k, %entry ]
  ret i32 %retval.0
}

