; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s
; RUN: %lli_comp -v %s

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

%struct.S = type { i32, i8, %struct.S* }

; CHECK-LABEL: @foo
define i32 @foo(i32 %i) {
entry:
; CHECK: alloca
; CHECK: call{{[a-z ]*}} void [[FName:@[_\.a-z0-9]+]]
  %s = alloca %struct.S, align 8
  %k = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 10, i32* %k, align 8
  %0 = load i32, i32* %k, align 8
  %add = add nsw i32 %0, %i
  store i32 %add, i32* %k, align 8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  store i8 73, i8* %c, align 8
  br label %if.end

if.end:
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %1 = load %struct.S*, %struct.S** %pS, align 8
  %cmp2 = icmp eq %struct.S* %1, %s
  %.call = select i1 %cmp2, i32 3, i32 %i
  br label %cleanup

cleanup:
  ret i32 %.call
}

; CHECK-LABEL: @bar
define i32 @bar(i32 %i) {
entry:
; CHECK: alloca
; CHECK: call{{[a-z ]*}} void [[FName]]
  %s = alloca %struct.S, align 8
  %k = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 10, i32* %k, align 8
  %0 = load i32, i32* %k, align 8
  %add = add nsw i32 %0, %i
  store i32 %add, i32* %k, align 8
  %c = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 1
  store i8 73, i8* %c, align 8
  br label %if.end

if.end:
  %pS = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 2
  %1 = load %struct.S*, %struct.S** %pS, align 8
  %cmp2 = icmp eq %struct.S* %1, %s
  %.call = select i1 %cmp2, i32 4, i32 %i
  br label %cleanup

cleanup:
  ret i32 %.call
}

define i32 @main() {
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar(i32 4)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  ret i32 0
}

declare i32 @printf(i8*, ...)

; CHECK: i32 [[FName]]
; CHECK-NOT: alloca