@g_globalValue = local_unnamed_addr global i32 0, align 4

define i32 @_Z2f0d(double %k) {
entry:
  tail call void @_Z16someCalculationsdi(double %k, i32 0) #2
  tail call void @_Z16someCalculationsdi(double %k, i32 1) #2
  %0 = load i32, i32* @g_globalValue, align 4
  %inc = add nsw i32 %0, 1
  store i32 %inc, i32* @g_globalValue, align 4
  ret i32 %inc
}

declare void @_Z16someCalculationsdi(double, i32)

define i32 @_Z2f1d(double %k) {
entry:
  %cmp = fcmp olt double %k, 0.000000e+00
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %sub = fsub double -0.000000e+00, %k
  %0 = load i32, i32* @g_globalValue, align 4
  tail call void @_Z16someCalculationsdi(double %sub, i32 %0) #2
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  tail call void @_Z16someCalculationsdi(double %k, i32 0) #2
  tail call void @_Z16someCalculationsdi(double %k, i32 1) #2
  %1 = load i32, i32* @g_globalValue, align 4
  %inc = add nsw i32 %1, 1
  store i32 %inc, i32* @g_globalValue, align 4
  ret i32 %inc
}
