; ModuleID = 'differentOutput.cpp'
source_filename = "differentOutput.cpp"
target datalayout = "e-m:w-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.0.24215"

; Function Attrs: nounwind uwtable
define i32 @"\01?foo@@YAHH@Z"(i32 %k) local_unnamed_addr #0 {
entry:
  %call = tail call i32 @"\01?someCalc@@YAHH@Z"(i32 %k) #2
  %conv = trunc i32 %call to i8
  %call1 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %k, i8 %conv) #2
  %conv2 = trunc i32 %call1 to i8
  %call3 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %call, i8 %conv2) #2
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

; Function Attrs: nounwind
declare i32 @"\01?someCalc@@YAHH@Z"(i32) local_unnamed_addr #1

; Function Attrs: nounwind
declare i32 @"\01?someCalc@@YAHHD@Z"(i32, i8) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define i32 @"\01?almostFoo@@YAHH@Z"(i32 %k) local_unnamed_addr #0 {
entry:
  %call = tail call i32 @"\01?someCalc@@YAHH@Z"(i32 %k) #2
  %conv = trunc i32 %call to i8
  %call1 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %k, i8 %conv) #2
  %conv2 = trunc i32 %call1 to i8
  %call3 = tail call i32 @"\01?someCalc@@YAHHD@Z"(i32 %call, i8 %conv2) #2
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

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"PIC Level", i32 2}
!1 = !{!"clang version 4.0.0 (trunk 290806)"}
