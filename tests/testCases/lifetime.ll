; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; RUN: %lli_comp -v %s

%struct.S = type { i32, i8, %struct.S* }
@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define void @init(%struct.S* dereferenceable(16)) {
entry:
  ret void
}

define i32 @foo(i32 %i) {
; CHECK-LABEL: @foo
entry:
  %s = alloca %struct.S, align 8
  %0 = bitcast %struct.S* %s to i8*
  call void @llvm.lifetime.start(i64 16, i8* nonnull %0)
  %j = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 %i, i32* %j, align 8
  %conv = trunc i32 %i to i8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  store i8 %conv, i8* %c, align 4
  %s1 = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  store %struct.S* null, %struct.S** %s1, align 8
  call void @init(%struct.S* nonnull dereferenceable(16) %s)
  %dummy0 = add i32 %i, 0
  %dummy1 = add i32 %i, 0
  %cmp = icmp slt i32 %i, 0
  br i1 %cmp, label %if.then, label %if.end

; CHECK: [[Al:%[_\.a-z0-9]+]] = alloca %struct.S
; CHECK-NEXT: [[BC:%[_\.a-z0-9]+]] = bitcast %struct.S* [[Al]] to i8*
; CHECK-NEXT: call void @llvm.lifetime.start{{[\.A-Za-z0-9]*}}(i64 {{[0-9]+}}, i8* nonnull [[BC]])
; Decide what to do with GEPInsts
; CHECK: call {{[a-z ]*}} i1  [[FName:@[_\.A-Za-z0-9]+]]

if.then:
  %1 = load i8, i8* %c, align 4
  %conv3 = sext i8 %1 to i32
  %mul = shl nsw i32 %conv3, 1
  br label %cleanup

if.end: 
  %2 = load i32, i32* %j, align 8
  %add = add nsw i32 %2, 10
  br label %cleanup

cleanup:
  %retval.0 = phi i32 [ %mul, %if.then ], [ %add, %if.end ]
  call void @llvm.lifetime.end(i64 16, i8* nonnull %0)
  ret i32 %retval.0
}

declare void @llvm.lifetime.start(i64, i8* nocapture)

declare void @llvm.lifetime.end(i64, i8* nocapture)

define i32 @bar(i32 %i) local_unnamed_addr {
; CHECK-LABEL: @bar
entry:
  %s = alloca %struct.S, align 8
  %0 = bitcast %struct.S* %s to i8*
  call void @llvm.lifetime.start(i64 16, i8* nonnull %0)
  %j = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 %i, i32* %j, align 8
  %conv = trunc i32 %i to i8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  store i8 %conv, i8* %c, align 4
  %s1 = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  store %struct.S* null, %struct.S** %s1, align 8
  call void @init (%struct.S* nonnull dereferenceable(16) %s)
  %dummy0 = add i32 %i, 0
  %dummy1 = add i32 %i, 0
  %cmp = icmp slt i32 %i, 0
  br i1 %cmp, label %if.then, label %if.end

; CHECK: call{{[a-z ]*}} i1  [[FName]]

if.then:
  %dummy2 = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  %1 = load i8, i8* %c, align 4
  %conv3 = sext i8 %1 to i32
  %mul = mul nsw i32 %conv3, 3
  br label %cleanup

if.end:
  %2 = load i32, i32* %j, align 8
  %add = add nsw i32 %2, 11
  br label %cleanup

cleanup:
  %retval.0 = phi i32 [ %mul, %if.then ], [ %add, %if.end ]
  call void @llvm.lifetime.end(i64 16, i8* nonnull %0)
  ret i32 %retval.0
}

define i32 @main() {
entry:
  %call = tail call i32 @foo(i32 10)
  %call1 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %call)
  %call2 = tail call i32 @bar(i32 11)
  %call3 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %call2)
  ret i32 0
}

; CHECK: define private {{[a-z]*}} i1 [[FName]](

declare i32 @printf(i8* nocapture readonly, ...)
