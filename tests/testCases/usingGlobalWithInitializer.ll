; check, that it optimizes without errors
; RUN: opt -S -load  %opt_path %pass_name %force_flag < %s | FileCheck %s
; Also test FunctionCost
; RUN: opt -S -load  %opt_path %pass_name < %s
; RUN: %lli_comp -v %s

@aux = internal global i32 0, align 4
@.str = private unnamed_addr constant [4 x i8] c"123\00", align 1
@f0 = local_unnamed_addr global i32 (i32)* null, align 8
@.str.2 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @_GLOBAL__sub_I_main.cpp, i8* null }]

define internal fastcc void @__cxx_global_var_init() section ".text.startup" {
entry:
  %call = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0))
  store i32 %call, i32* @aux, align 4
  %0 = tail call {}* @llvm.invariant.start.p0i8(i64 4, i8* bitcast (i32* @aux to i8*))
  ret void
}

declare i32 @printf(i8* nocapture readonly, ...)

declare {}* @llvm.invariant.start.p0i8(i64, i8* nocapture)

define internal fastcc void @__cxx_global_var_init.1() section ".text.startup" {
entry:
  %0 = load i32, i32* @aux, align 4
  %cmp = icmp sgt i32 %0, 0
  %cond = select i1 %cmp, i32 (i32)* @foo, i32 (i32)* @bar
  store i32 (i32)* %cond, i32 (i32)** @f0, align 8
  ret void
}

; CHECK-LABEL: define i32 @foo
define i32 @foo(i32 %j) {
entry:
  %0 = load i32 (i32)*, i32 (i32)** @f0, align 8
  %cmp = icmp eq i32 (i32)* %0, @foo
  br i1 %cmp, label %if.end, label %if.then
if.then:
  %call = tail call i32 %0(i32 4)
  br label %if.end
if.end:
  %cmp1 = icmp sgt i32 %j, 0
  br i1 %cmp1, label %if.then2, label %return
if.then2:
; CHECK: if.then2
; CHECK: call {{[a-z ]*}} [[FName:@[_\.A-Za-z0-9]+]]
  %1 = load i32, i32* @aux, align 4
  %mul = shl i32 %1, 1
  %add = add nsw i32 %mul, 3
  %call3 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.2, i64 0, i64 0), i32 %add)
  br label %return

return:
  %retval.0 = phi i32 [ %j, %if.then2 ], [ 0, %if.end ]
  ret i32 %retval.0
}

; CHECK-LABEL: define i32 @bar
define i32 @bar(i32 %i) {
entry:
  %0 = load i32 (i32)*, i32 (i32)** @f0, align 8
  %cmp = icmp eq i32 (i32)* %0, @bar
  br i1 %cmp, label %if.end, label %if.then

if.then:
  %call = tail call i32 %0(i32 4)
  br label %if.end

if.end:
  %tobool = icmp eq i32 %i, 0
  br i1 %tobool, label %return, label %if.then1

if.then1:
; CHECK: call{{[a-z ]*}} [[FName]]
  %1 = load i32, i32* @aux, align 4
  %mul = shl i32 %1, 1
  %add = add nsw i32 %mul, 3
  %call2 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.2, i64 0, i64 0), i32 %add)
  br label %return

return:
  %retval.0 = phi i32 [ %i, %if.then1 ], [ 1, %if.end ]
  ret i32 %retval.0
}

; CHECK-LABEL: @main
define i32 @main() {
  %call1 = call i32 @foo(i32 3)
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call1)
  %call3 = call i32 @bar(i32 4)
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %call3)
  ret i32 0
}

define internal void @_GLOBAL__sub_I_main.cpp() section ".text.startup" {
entry:
  tail call fastcc void @__cxx_global_var_init()
  tail call fastcc void @__cxx_global_var_init.1()
  ret void
}

; CHECK: define {{[ a-z]*}} [[FName]]