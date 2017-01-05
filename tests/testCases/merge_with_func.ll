; ModuleID = 'main.c'
source_filename = "main.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: norecurse nounwind readnone uwtable
define i32 @foo(i32 %i, i32 %j) local_unnamed_addr #0 {
entry:
  %mul = mul nsw i32 %i, %i
  %mul1 = mul nsw i32 %j, %j
  %add = add nuw nsw i32 %mul1, %mul
  %sub = sub nsw i32 %mul, %mul1
  %mul4 = mul nsw i32 %add, %sub
  ret i32 %mul4
}

; Function Attrs: norecurse nounwind readnone uwtable
define i32 @bar(i32 %i, i32 %j, i32 %k) local_unnamed_addr #0 {
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

attributes #0 = { norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.0 (trunk 290798)"}
