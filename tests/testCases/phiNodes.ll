; ModuleID = 'main.cpp'
source_filename = "main.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind readonly uwtable
define i32 @_Z5func1idPc(i32 %i1, double %d2, i8* nocapture readonly %c4) local_unnamed_addr #0 {
entry:
  %call = tail call i64 @strlen(i8* %c4) #5
  %conv = trunc i64 %call to i32
  %cmp11 = icmp sgt i32 %conv, 0
  br i1 %cmp11, label %for.body.preheader, label %for.cond.cleanup

for.body.preheader:                               ; preds = %entry
  %wide.trip.count = and i64 %call, 4294967295
  %min.iters.check = icmp ult i64 %wide.trip.count, 8
  br i1 %min.iters.check, label %for.body.preheader20, label %min.iters.checked

for.body.preheader20:                             ; preds = %middle.block, %min.iters.checked, %for.body.preheader
  %indvars.iv.ph = phi i64 [ 0, %min.iters.checked ], [ 0, %for.body.preheader ], [ %n.vec, %middle.block ]
  %result.012.ph = phi i32 [ 0, %min.iters.checked ], [ 0, %for.body.preheader ], [ %9, %middle.block ]
  br label %for.body

min.iters.checked:                                ; preds = %for.body.preheader
  %n.mod.vf = and i64 %call, 7
  %n.vec = sub nsw i64 %wide.trip.count, %n.mod.vf
  %cmp.zero = icmp eq i64 %n.vec, 0
  br i1 %cmp.zero, label %for.body.preheader20, label %vector.body.preheader

vector.body.preheader:                            ; preds = %min.iters.checked
  br label %vector.body

vector.body:                                      ; preds = %vector.body.preheader, %vector.body
  %index = phi i64 [ %index.next, %vector.body ], [ 0, %vector.body.preheader ]
  %vec.phi = phi <4 x i32> [ %6, %vector.body ], [ zeroinitializer, %vector.body.preheader ]
  %vec.phi15 = phi <4 x i32> [ %7, %vector.body ], [ zeroinitializer, %vector.body.preheader ]
  %0 = getelementptr inbounds i8, i8* %c4, i64 %index
  %1 = bitcast i8* %0 to <4 x i8>*
  %wide.load = load <4 x i8>, <4 x i8>* %1, align 1, !tbaa !1
  %2 = getelementptr i8, i8* %0, i64 4
  %3 = bitcast i8* %2 to <4 x i8>*
  %wide.load16 = load <4 x i8>, <4 x i8>* %3, align 1, !tbaa !1
  %4 = sext <4 x i8> %wide.load to <4 x i32>
  %5 = sext <4 x i8> %wide.load16 to <4 x i32>
  %6 = add nsw <4 x i32> %4, %vec.phi
  %7 = add nsw <4 x i32> %5, %vec.phi15
  %index.next = add i64 %index, 8
  %8 = icmp eq i64 %index.next, %n.vec
  br i1 %8, label %middle.block, label %vector.body, !llvm.loop !4

middle.block:                                     ; preds = %vector.body
  %bin.rdx = add <4 x i32> %7, %6
  %rdx.shuf = shufflevector <4 x i32> %bin.rdx, <4 x i32> undef, <4 x i32> <i32 2, i32 3, i32 undef, i32 undef>
  %bin.rdx17 = add <4 x i32> %bin.rdx, %rdx.shuf
  %rdx.shuf18 = shufflevector <4 x i32> %bin.rdx17, <4 x i32> undef, <4 x i32> <i32 1, i32 undef, i32 undef, i32 undef>
  %bin.rdx19 = add <4 x i32> %bin.rdx17, %rdx.shuf18
  %9 = extractelement <4 x i32> %bin.rdx19, i32 0
  %cmp.n = icmp eq i64 %n.mod.vf, 0
  br i1 %cmp.n, label %for.cond.cleanup, label %for.body.preheader20

for.cond.cleanup.loopexit:                        ; preds = %for.body
  br label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond.cleanup.loopexit, %middle.block, %entry
  %result.0.lcssa = phi i32 [ 0, %entry ], [ %9, %middle.block ], [ %add, %for.cond.cleanup.loopexit ]
  %conv2 = fptosi double %d2 to i32
  %add3 = add nsw i32 %result.0.lcssa, %conv2
  %mul = mul nsw i32 %add3, %i1
  ret i32 %mul

for.body:                                         ; preds = %for.body.preheader20, %for.body
  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body ], [ %indvars.iv.ph, %for.body.preheader20 ]
  %result.012 = phi i32 [ %add, %for.body ], [ %result.012.ph, %for.body.preheader20 ]
  %arrayidx = getelementptr inbounds i8, i8* %c4, i64 %indvars.iv
  %10 = load i8, i8* %arrayidx, align 1, !tbaa !1
  %conv1 = sext i8 %10 to i32
  %add = add nsw i32 %conv1, %result.012
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond, label %for.cond.cleanup.loopexit, label %for.body, !llvm.loop !7
}

; Function Attrs: nounwind readonly
declare i64 @strlen(i8* nocapture) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define i32 @_Z5func2idPc(i32 %i1, double %d2, i8* nocapture readonly %c4) local_unnamed_addr #2 {
entry:
  %call = tail call i32 @_Z8SomeFunci(i32 %i1) #6
  %cmp = icmp slt i32 %call, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %call1 = tail call i64 @strlen(i8* %c4) #5
  %conv = trunc i64 %call1 to i32
  %cmp215 = icmp sgt i32 %conv, 0
  br i1 %cmp215, label %for.body.preheader, label %for.cond.cleanup

for.body.preheader:                               ; preds = %if.end
  %wide.trip.count = and i64 %call1, 4294967295
  %min.iters.check = icmp ult i64 %wide.trip.count, 8
  br i1 %min.iters.check, label %for.body.preheader24, label %min.iters.checked

for.body.preheader24:                             ; preds = %middle.block, %min.iters.checked, %for.body.preheader
  %indvars.iv.ph = phi i64 [ 0, %min.iters.checked ], [ 0, %for.body.preheader ], [ %n.vec, %middle.block ]
  %result.016.ph = phi i32 [ 0, %min.iters.checked ], [ 0, %for.body.preheader ], [ %9, %middle.block ]
  br label %for.body

min.iters.checked:                                ; preds = %for.body.preheader
  %n.mod.vf = and i64 %call1, 7
  %n.vec = sub nsw i64 %wide.trip.count, %n.mod.vf
  %cmp.zero = icmp eq i64 %n.vec, 0
  br i1 %cmp.zero, label %for.body.preheader24, label %vector.body.preheader

vector.body.preheader:                            ; preds = %min.iters.checked
  br label %vector.body

vector.body:                                      ; preds = %vector.body.preheader, %vector.body
  %index = phi i64 [ %index.next, %vector.body ], [ 0, %vector.body.preheader ]
  %vec.phi = phi <4 x i32> [ %6, %vector.body ], [ zeroinitializer, %vector.body.preheader ]
  %vec.phi19 = phi <4 x i32> [ %7, %vector.body ], [ zeroinitializer, %vector.body.preheader ]
  %0 = getelementptr inbounds i8, i8* %c4, i64 %index
  %1 = bitcast i8* %0 to <4 x i8>*
  %wide.load = load <4 x i8>, <4 x i8>* %1, align 1, !tbaa !1
  %2 = getelementptr i8, i8* %0, i64 4
  %3 = bitcast i8* %2 to <4 x i8>*
  %wide.load20 = load <4 x i8>, <4 x i8>* %3, align 1, !tbaa !1
  %4 = sext <4 x i8> %wide.load to <4 x i32>
  %5 = sext <4 x i8> %wide.load20 to <4 x i32>
  %6 = add nsw <4 x i32> %4, %vec.phi
  %7 = add nsw <4 x i32> %5, %vec.phi19
  %index.next = add i64 %index, 8
  %8 = icmp eq i64 %index.next, %n.vec
  br i1 %8, label %middle.block, label %vector.body, !llvm.loop !9

middle.block:                                     ; preds = %vector.body
  %bin.rdx = add <4 x i32> %7, %6
  %rdx.shuf = shufflevector <4 x i32> %bin.rdx, <4 x i32> undef, <4 x i32> <i32 2, i32 3, i32 undef, i32 undef>
  %bin.rdx21 = add <4 x i32> %bin.rdx, %rdx.shuf
  %rdx.shuf22 = shufflevector <4 x i32> %bin.rdx21, <4 x i32> undef, <4 x i32> <i32 1, i32 undef, i32 undef, i32 undef>
  %bin.rdx23 = add <4 x i32> %bin.rdx21, %rdx.shuf22
  %9 = extractelement <4 x i32> %bin.rdx23, i32 0
  %cmp.n = icmp eq i64 %n.mod.vf, 0
  br i1 %cmp.n, label %for.cond.cleanup, label %for.body.preheader24

for.cond.cleanup.loopexit:                        ; preds = %for.body
  br label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond.cleanup.loopexit, %middle.block, %if.end
  %result.0.lcssa = phi i32 [ 0, %if.end ], [ %9, %middle.block ], [ %add, %for.cond.cleanup.loopexit ]
  %conv4 = fptosi double %d2 to i32
  %add5 = add nsw i32 %result.0.lcssa, %conv4
  %mul = mul nsw i32 %add5, %i1
  br label %cleanup

for.body:                                         ; preds = %for.body.preheader24, %for.body
  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body ], [ %indvars.iv.ph, %for.body.preheader24 ]
  %result.016 = phi i32 [ %add, %for.body ], [ %result.016.ph, %for.body.preheader24 ]
  %arrayidx = getelementptr inbounds i8, i8* %c4, i64 %indvars.iv
  %10 = load i8, i8* %arrayidx, align 1, !tbaa !1
  %conv3 = sext i8 %10 to i32
  %add = add nsw i32 %conv3, %result.016
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond, label %for.cond.cleanup.loopexit, label %for.body, !llvm.loop !10

cleanup:                                          ; preds = %entry, %for.cond.cleanup
  %retval.0 = phi i32 [ %mul, %for.cond.cleanup ], [ 0, %entry ]
  ret i32 %retval.0
}

; Function Attrs: nounwind
declare i32 @_Z8SomeFunci(i32) local_unnamed_addr #3

; Function Attrs: norecurse nounwind readnone uwtable
define i32 @main() local_unnamed_addr #4 {
entry:
  ret i32 0
}

attributes #0 = { nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind readonly }
attributes #6 = { nounwind }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.0 (trunk 285500)"}
!1 = !{!2, !2, i64 0}
!2 = !{!"omnipotent char", !3, i64 0}
!3 = !{!"Simple C++ TBAA"}
!4 = distinct !{!4, !5, !6}
!5 = !{!"llvm.loop.vectorize.width", i32 1}
!6 = !{!"llvm.loop.interleave.count", i32 1}
!7 = distinct !{!7, !8, !5, !6}
!8 = !{!"llvm.loop.unroll.runtime.disable"}
!9 = distinct !{!9, !5, !6}
!10 = distinct !{!10, !8, !5, !6}
