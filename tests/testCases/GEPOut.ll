; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; RUN: %lli_comp -v %s

%struct.S = type { i32, i32 }
@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i32 @foo(i32 %i) {
; CHECK-LABEL: @foo
entry:
  ; CHECK: call{{[a-z ]*}} {{i32*|void}} [[FName:@[_\.A-Za-z0-9]+]]
  %s = alloca %struct.S, align 8
  %j = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 %i, i32* %j, align 8
  %dummy0 = add i32 %i, 0
  %dummy1 = add i32 %i, 0
  %dummy2 = add i32 %i, 0
  %dummy3 = add i32 %i, 0
  br label %end
end:
  %k = load i32, i32* %j, align 8
  ret i32 %k
}

define i32 @bar(i32 %i) {
; CHECK-LABEL: @bar
entry:
  ; CHECK: call{{[a-z ]*}} {{i32*|void}} [[FName]]
  %s = alloca %struct.S, align 8
  %j = getelementptr inbounds %struct.S, %struct.S* %s, i64 0, i32 0
  store i32 %i, i32* %j, align 8
  %dummy0 = add i32 %i, 0
  %dummy1 = add i32 %i, 0
  %dummy2 = add i32 %i, 0
  %dummy3 = add i32 %i, 0
  br label %end
end:
  %k = load i32, i32* %j, align 8
  %r = add nsw i32 %k, %i
  ret i32 %r
}

define i32 @main() {
entry:
  %call = tail call i32 @foo(i32 10)
  %call1 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %call)
  %call2 = tail call i32 @bar(i32 11)
  %call3 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %call2)
  ret i32 0
}

; CHECK: define private {{[a-z]*}} {{i32*|void}} [[FName]](

declare i32 @printf(i8* nocapture readonly, ...)
