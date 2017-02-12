define i32 @_Z5func1idPc(i32 %i1, double %d2, i8* nocapture readonly %c4) {
entry:
  %conv = fptosi double %d2 to i32
  %add = add nsw i32 %conv, %i1
  %conv14 = zext i32 %add to i64
  %call = tail call i64 @strlen(i8* %c4) #5
  %add2 = add i64 %call, %conv14
  %conv3 = trunc i64 %add2 to i32
  ret i32 %conv3
}

declare i64 @strlen(i8* nocapture)

define i32 @_Z5func2idPc(i32 %i1, double %d2, i8* nocapture readonly %c4) {
entry:
  %call = tail call i32 @_Z8SomeFunci(i32 %i1) #6
  %cmp = icmp slt i32 %call, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %conv = fptosi double %d2 to i32
  %add = add nsw i32 %call, %conv
  %conv17 = zext i32 %add to i64
  %call2 = tail call i64 @strlen(i8* %c4) #5
  %add3 = add i64 %call2, %conv17
  %conv4 = trunc i64 %add3 to i32
  br label %cleanup

cleanup:                                          ; preds = %entry, %if.end
  %retval.0 = phi i32 [ %conv4, %if.end ], [ 0, %entry ]
  ret i32 %retval.0
}

declare i32 @_Z8SomeFunci(i32)

define i32 @main() {
entry:
  ret i32 0
}
