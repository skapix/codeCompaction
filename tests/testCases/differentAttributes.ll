; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; RUN: %lli_comp -v %s

@f0 = local_unnamed_addr global i32 (i32)* null, align 8
@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@.str.1 = private unnamed_addr constant [5 x i8] c"123\0A\00", align 1

; CHECK-LABEL: @foo
define i32 @foo(i32 %j) #0 {
entry:
  %0 = load i32 (i32)*, i32 (i32)** @f0, align 8
  %cmp = icmp eq i32 (i32)* %0, @foo
  br i1 %cmp, label %if.end, label %if.then

if.then:
  %call = tail call i32 %0(i32 4)
  br label %return

if.end: 
  %cmp1 = icmp sgt i32 %j, 0
  br i1 %cmp1, label %if.then2, label %if.else

if.then2:
  %mul = shl i32 %j, 1
  %add = add nsw i32 %mul, 3
  %call3 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %add)
  %add4 = add nsw i32 %call3, %j
  br label %return

if.else: 
; CHECK: if.else:
; CHECK-NEXT: call{{[a-z ]*}} i32 [[FName:@[_\.A-Za-z0-9]+]]
  %mul5 = mul nsw i32 %j, %j
  %mul615 = add nuw i32 %mul5, 3
  %add8 = mul i32 %mul615, %j
  %add9 = add nsw i32 %add8, 2
  br label %return

return:
  %retval.0 = phi i32 [ %call, %if.then ], [ %add4, %if.then2 ], [ %add9, %if.else ]
  ret i32 %retval.0
}

declare i32 @printf(i8* nocapture readonly, ...)

; CHECK-LABEL: @bar
define i32 @bar(i32 %i) #2 {
entry:
  %cmp = icmp eq i32 %i, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:
; CHECK: if.then:
; CHECK-NEXT: call{{[a-z ]*}} i32 [[FName:@[_\.A-Za-z0-9]+]]
  %mul = mul nsw i32 %i, %i
  %mul19 = add nuw i32 %mul, 3
  %add = mul i32 %mul19, %i
  %add3 = add nsw i32 %add, 2
  br label %if.end

if.else:
  %call = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.1, i64 0, i64 0))
  br label %if.end

if.end:
  %k.0 = phi i32 [ %add3, %if.then ], [ %call, %if.else ]
  ret i32 %k.0
}

define i32 @main() {
entry:
  store i32 (i32)* @foo, i32 (i32)** @f0, align 8
  %call = tail call i32 @foo(i32 2)
  store i32 (i32)* @bar, i32 (i32)** @f0, align 8
  %call1 = tail call i32 @foo(i32 2)
  %call2 = tail call i32 @bar(i32 1)
  ret i32 0
}

; CHECK: i32 [[FName]]

attributes #0 = { uwtable "disable-tail-calls"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { norecurse nounwind "disable-tail-calls"="true" "stack-protector-buffer-size"="16" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }