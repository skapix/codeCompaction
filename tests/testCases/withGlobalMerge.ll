; ModuleID = 'main.cpp'
source_filename = "main.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@g_globalValue = local_unnamed_addr global i32 0, align 4

; Function Attrs: nounwind optsize uwtable
define i32 @_Z2f0d(double %k) local_unnamed_addr #0 {
entry:
  tail call void @_Z16someCalculationsdi(double %k, i32 0) #2
  tail call void @_Z16someCalculationsdi(double %k, i32 1) #2
  %0 = load i32, i32* @g_globalValue, align 4, !tbaa !1
  %inc = add nsw i32 %0, 1
  store i32 %inc, i32* @g_globalValue, align 4, !tbaa !1
  ret i32 %inc
}

; Function Attrs: nounwind optsize
declare void @_Z16someCalculationsdi(double, i32) local_unnamed_addr #1

; Function Attrs: nounwind optsize uwtable
define i32 @_Z2f1d(double %k) local_unnamed_addr #0 {
entry:
  %cmp = fcmp olt double %k, 0.000000e+00
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %sub = fsub double -0.000000e+00, %k
  %0 = load i32, i32* @g_globalValue, align 4, !tbaa !1
  tail call void @_Z16someCalculationsdi(double %sub, i32 %0) #2
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  tail call void @_Z16someCalculationsdi(double %k, i32 0) #2
  tail call void @_Z16someCalculationsdi(double %k, i32 1) #2
  %1 = load i32, i32* @g_globalValue, align 4, !tbaa !1
  %inc = add nsw i32 %1, 1
  store i32 %inc, i32* @g_globalValue, align 4, !tbaa !1
  ret i32 %inc
}

attributes #0 = { nounwind optsize uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind optsize "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind optsize }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.0 (trunk 290798)"}
!1 = !{!2, !2, i64 0}
!2 = !{!"int", !3, i64 0}
!3 = !{!"omnipotent char", !4, i64 0}
!4 = !{!"Simple C++ TBAA"}
