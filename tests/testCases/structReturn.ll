; ModuleID = 'structReturn.cpp'
source_filename = "structReturn.cpp"
target datalayout = "e-m:w-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.0.24215"

%struct.S = type { i32, i8, %struct.S* }

; Function Attrs: uwtable
define i32 @"\01?foo@@YAHXZ"() local_unnamed_addr #0 {
entry:
  %s = alloca %struct.S, align 8
  %0 = bitcast %struct.S* %s to i8*
  call void @llvm.lifetime.start(i64 16, i8* nonnull %0) #3
  call void @"\01?init@@YAXAEAUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  %i = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  %1 = load i32, i32* %i, align 8, !tbaa !2
  %add = add nsw i32 %1, 10
  store i32 %add, i32* %i, align 8, !tbaa !2
  %call = call i32 @"\01?someCalc@@YAHAEBUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  store i32 %call, i32* %i, align 8, !tbaa !2
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  %2 = load i8, i8* %c, align 4, !tbaa !8
  %cmp = icmp eq i8 %2, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %3 = load %struct.S*, %struct.S** %pS, align 8, !tbaa !9
  %cmp2 = icmp eq %struct.S* %3, %s
  %.call = select i1 %cmp2, i32 3, i32 %call
  br label %cleanup

cleanup:                                          ; preds = %if.end, %entry
  %retval.0 = phi i32 [ 2, %entry ], [ %.call, %if.end ]
  call void @llvm.lifetime.end(i64 16, i8* nonnull %0) #3
  ret i32 %retval.0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

declare void @"\01?init@@YAXAEAUS@@@Z"(%struct.S* dereferenceable(16)) local_unnamed_addr #2

declare i32 @"\01?someCalc@@YAHAEBUS@@@Z"(%struct.S* dereferenceable(16)) local_unnamed_addr #2

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

; Function Attrs: uwtable
define i32 @"\01?almostFoo@@YAHXZ"() local_unnamed_addr #0 {
entry:
  %s = alloca %struct.S, align 8
  %0 = bitcast %struct.S* %s to i8*
  call void @llvm.lifetime.start(i64 16, i8* nonnull %0) #3
  call void @"\01?init@@YAXAEAUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  %i = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  %1 = load i32, i32* %i, align 8, !tbaa !2
  %add = add nsw i32 %1, 10
  store i32 %add, i32* %i, align 8, !tbaa !2
  %call = call i32 @"\01?someCalc@@YAHAEBUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  store i32 %call, i32* %i, align 8, !tbaa !2
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  %2 = load i8, i8* %c, align 4, !tbaa !8
  %cmp = icmp eq i8 %2, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %3 = load %struct.S*, %struct.S** %pS, align 8, !tbaa !9
  %cmp2 = icmp eq %struct.S* %3, %s
  %.call = select i1 %cmp2, i32 4, i32 %call
  br label %cleanup

cleanup:                                          ; preds = %if.end, %entry
  %retval.0 = phi i32 [ 3, %entry ], [ %.call, %if.end ]
  call void @llvm.lifetime.end(i64 16, i8* nonnull %0) #3
  ret i32 %retval.0
}

attributes #0 = { uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"PIC Level", i32 2}
!1 = !{!"clang version 4.0.0 (trunk 290806)"}
!2 = !{!3, !4, i64 0}
!3 = !{!"?AUS@@", !4, i64 0, !5, i64 4, !7, i64 8}
!4 = !{!"int", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C++ TBAA"}
!7 = !{!"any pointer", !5, i64 0}
!8 = !{!3, !5, i64 4}
!9 = !{!3, !7, i64 8}
