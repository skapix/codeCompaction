%struct.S = type { i32, i8, %struct.S* }

define i32 @"\01?foo@@YAHXZ"() {
entry:
  %s = alloca %struct.S, align 8
  %0 = bitcast %struct.S* %s to i8*
  call void @llvm.lifetime.start(i64 16, i8* nonnull %0)
  call void @"\01?init@@YAXAEAUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  %i = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  %1 = load i32, i32* %i, align 8
  %add = add nsw i32 %1, 10
  store i32 %add, i32* %i, align 8
  %call = call i32 @"\01?someCalc@@YAHAEBUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  store i32 %call, i32* %i, align 8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  %2 = load i8, i8* %c, align 4
  %cmp = icmp eq i8 %2, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %3 = load %struct.S*, %struct.S** %pS, align 8
  %cmp2 = icmp eq %struct.S* %3, %s
  %.call = select i1 %cmp2, i32 3, i32 %call
  br label %cleanup

cleanup:                                          ; preds = %if.end, %entry
  %retval.0 = phi i32 [ 2, %entry ], [ %.call, %if.end ]
  call void @llvm.lifetime.end(i64 16, i8* nonnull %0)
  ret i32 %retval.0
}

declare void @llvm.lifetime.start(i64, i8* nocapture)

declare void @"\01?init@@YAXAEAUS@@@Z"(%struct.S* dereferenceable(16))

declare i32 @"\01?someCalc@@YAHAEBUS@@@Z"(%struct.S* dereferenceable(16))

declare void @llvm.lifetime.end(i64, i8* nocapture)

define i32 @"\01?almostFoo@@YAHXZ"() {
entry:
  %s = alloca %struct.S, align 8
  %0 = bitcast %struct.S* %s to i8*
  call void @llvm.lifetime.start(i64 16, i8* nonnull %0) 
  call void @"\01?init@@YAXAEAUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  %i = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  %1 = load i32, i32* %i, align 8
  %add = add nsw i32 %1, 10
  store i32 %add, i32* %i, align 8
  %call = call i32 @"\01?someCalc@@YAHAEBUS@@@Z"(%struct.S* nonnull dereferenceable(16) %s)
  store i32 %call, i32* %i, align 8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  %2 = load i8, i8* %c, align 4
  %cmp = icmp eq i8 %2, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %3 = load %struct.S*, %struct.S** %pS, align 8
  %cmp2 = icmp eq %struct.S* %3, %s
  %.call = select i1 %cmp2, i32 4, i32 %call
  br label %cleanup

cleanup:                                          ; preds = %if.end, %entry
  %retval.0 = phi i32 [ 3, %entry ], [ %.call, %if.end ]
  call void @llvm.lifetime.end(i64 16, i8* nonnull %0)
  ret i32 %retval.0
}
