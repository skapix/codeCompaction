%struct.S = type { i32, i8, %struct.S* }

define i32 @foo(i32 %i) {
entry:
  %s = alloca %struct.S, align 8
  %k = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 10, i32* %k, align 8
  %0 = load i32, i32* %k, align 8
  %add = add nsw i32 %0, %i
  store i32 %add, i32* %k, align 8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  store i8 73, i8* %c, align 8
  %cmp = icmp eq i32 %i, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %1 = load %struct.S*, %struct.S** %pS, align 8
  %cmp2 = icmp eq %struct.S* %1, %s
  %.call = select i1 %cmp2, i32 3, i32 %i
  br label %cleanup

cleanup:
  %retval.0 = phi i32 [ 2, %entry ], [ %.call, %if.end ]
  ret i32 %retval.0
}

define i32 @bar(i32 %i) {
entry:
  %s = alloca %struct.S, align 8
  %k = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 10, i32* %k, align 8
  %0 = load i32, i32* %k, align 8
  %add = add nsw i32 %0, %i
  store i32 %add, i32* %k, align 8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  store i8 73, i8* %c, align 8
  %cmp = icmp eq i32 %i, 0
  br i1 %cmp, label %cleanup, label %if.end

if.end:
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %1 = load %struct.S*, %struct.S** %pS, align 8
  %cmp2 = icmp eq %struct.S* %1, %s
  %.call = select i1 %cmp2, i32 4, i32 %i
  br label %cleanup

cleanup:
  %retval.0 = phi i32 [ 3, %entry ], [ %.call, %if.end ]
  ret i32 %retval.0
}
