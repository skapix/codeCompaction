; RUN: opt -S -load  %opt_path -bbfactor -bbfactor-force-merging < %s
; check, that it optimizes without errors

%"class.std::exception" = type { i32 (...)** }

@_ZTISt9exception = external constant i8*
@.str = private unnamed_addr constant [8 x i8] c"%d: %s\0A\00", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@_ZTVSt9exception = external unnamed_addr constant { [5 x i8*] }, align 8

define i32 @throws(i32 %i) {
entry:
  %cmp = icmp slt i32 %i, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %exception = tail call i8* @__cxa_allocate_exception(i64 8)
  %0 = bitcast i8* %exception to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTVSt9exception, i64 0, inrange i32 0, i64 2) to i32 (...)**), i32 (...)*** %0, align 8
  tail call void @__cxa_throw(i8* %exception, i8* bitcast (i8** @_ZTISt9exception to i8*), i8* bitcast (void (%"class.std::exception"*)* @_ZNSt9exceptionD1Ev to i8*))
  unreachable

if.end:
  %sub = sub nsw i32 0, %i
  ret i32 %sub
}

declare i8* @__cxa_allocate_exception(i64) local_unnamed_addr

declare void @_ZNSt9exceptionD1Ev(%"class.std::exception"*) unnamed_addr

declare void @__cxa_throw(i8*, i8*, i8*) local_unnamed_addr

define i32 @foo(i32 %i) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %mul = mul nsw i32 %i, %i
  %sub = sub nsw i32 %mul, %i
  %cmp.i = icmp slt i32 %sub, 1
  br i1 %cmp.i, label %if.then.i, label %invoke.cont

if.then.i:
  %exception.i = tail call i8* @__cxa_allocate_exception(i64 8)
  %0 = bitcast i8* %exception.i to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTVSt9exception, i64 0, inrange i32 0, i64 2) to i32 (...)**), i32 (...)*** %0, align 8
  invoke void @__cxa_throw(i8* %exception.i, i8* bitcast (i8** @_ZTISt9exception to i8*), i8* bitcast (void (%"class.std::exception"*)* @_ZNSt9exceptionD1Ev to i8*))
          to label %.noexc unwind label %lpad

.noexc:
  unreachable

invoke.cont:
  %sub.i = sub i32 1, %sub
  %sub9 = mul i32 %sub.i, %sub
  %add10 = add nsw i32 %sub9, 2
  %cmp.i60 = icmp slt i32 %sub9, -2
  br i1 %cmp.i60, label %if.then.i62, label %invoke.cont13

if.then.i62:
  %exception.i61 = tail call i8* @__cxa_allocate_exception(i64 8)
  %1 = bitcast i8* %exception.i61 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTVSt9exception, i64 0, inrange i32 0, i64 2) to i32 (...)**), i32 (...)*** %1, align 8
  invoke void @__cxa_throw(i8* %exception.i61, i8* bitcast (i8** @_ZTISt9exception to i8*), i8* bitcast (void (%"class.std::exception"*)* @_ZNSt9exceptionD1Ev to i8*))
          to label %.noexc64 unwind label %lpad12

.noexc64:
  unreachable

lpad:
  %2 = landingpad { i8*, i32 }
          cleanup
          catch i8* bitcast (i8** @_ZTISt9exception to i8*)
  %3 = extractvalue { i8*, i32 } %2, 0
  %4 = extractvalue { i8*, i32 } %2, 1
  %5 = tail call i32 @llvm.eh.typeid.for(i8* bitcast (i8** @_ZTISt9exception to i8*))
  %matches = icmp eq i32 %4, %5
  br i1 %matches, label %catch, label %ehcleanup

catch:
  %6 = tail call i8* @__cxa_begin_catch(i8* %3)
  %exn.byref = bitcast i8* %6 to %"class.std::exception"*
  %mul2 = mul nsw i32 %i, 3
  %add = add nsw i32 %mul2, 4
  %7 = bitcast i8* %6 to i8* (%"class.std::exception"*)***
  %vtable = load i8* (%"class.std::exception"*)**, i8* (%"class.std::exception"*)*** %7, align 8
  %vfn = getelementptr inbounds i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vtable, i64 2
  %8 = load i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vfn, align 8
  %call3 = tail call i8* %8(%"class.std::exception"* %exn.byref)
  %call6 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str, i64 0, i64 0), i32 %add, i8* %call3)
  tail call void @__cxa_end_catch()
  br label %cleanup

invoke.cont13:
  %sub.i63 = sub i32 -2, %sub9
  %cmp.i66 = icmp slt i32 %sub.i63, 0
  br i1 %cmp.i66, label %if.then.i68, label %cleanup

if.then.i68:
  %exception.i67 = tail call i8* @__cxa_allocate_exception(i64 8)
  %9 = bitcast i8* %exception.i67 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTVSt9exception, i64 0, inrange i32 0, i64 2) to i32 (...)**), i32 (...)*** %9, align 8
  invoke void @__cxa_throw(i8* %exception.i67, i8* bitcast (i8** @_ZTISt9exception to i8*), i8* bitcast (void (%"class.std::exception"*)* @_ZNSt9exceptionD1Ev to i8*))
          to label %.noexc69 unwind label %lpad15

.noexc69:
  unreachable

lpad12:
  %10 = landingpad { i8*, i32 }
          cleanup
          catch i8* bitcast (i8** @_ZTISt9exception to i8*)
  %nop0 = add i1 0, 0
  %nop1 = add i1 0, 0
  %nop2 = add i1 0, 0
  %nop3 = add i1 0, 0
  br label %catch.dispatch18

lpad15:
  %11 = landingpad { i8*, i32 }
          cleanup
          catch i8* bitcast (i8** @_ZTISt9exception to i8*)
  %nop00 = add i1 0, 0
  %nop20 = add i1 0, 0
  %nop30 = add i1 0, 0
  %nop40 = add i1 0, 0
  br label %catch.dispatch18

catch.dispatch18:
  %.sink59 = phi { i8*, i32 } [ %11, %lpad15 ], [ %10, %lpad12 ]
  %12 = extractvalue { i8*, i32 } %.sink59, 0
  %13 = extractvalue { i8*, i32 } %.sink59, 1
  %14 = tail call i32 @llvm.eh.typeid.for(i8* bitcast (i8** @_ZTISt9exception to i8*))
  %matches20 = icmp eq i32 %13, %14
  br i1 %matches20, label %catch21, label %ehcleanup

catch21:
  %15 = tail call i8* @__cxa_begin_catch(i8* %12)
  %exn.byref24 = bitcast i8* %15 to %"class.std::exception"*
  %mul25 = mul nsw i32 %add10, 3
  %add26 = add nsw i32 %mul25, 4
  %16 = bitcast i8* %15 to i8* (%"class.std::exception"*)***
  %vtable27 = load i8* (%"class.std::exception"*)**, i8* (%"class.std::exception"*)*** %16, align 8
  %vfn28 = getelementptr inbounds i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vtable27, i64 2
  %17 = load i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vfn28, align 8
  %call29 = tail call i8* %17(%"class.std::exception"* %exn.byref24)
  %call32 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str, i64 0, i64 0), i32 %add26, i8* %call29)
  tail call void @__cxa_end_catch()
  br label %cleanup

cleanup:
  %retval.0 = phi i32 [ 1, %catch21 ], [ 1, %catch ], [ %add10, %invoke.cont13 ]
  ret i32 %retval.0

ehcleanup:
  %ehselector.slot.1 = phi i32 [ %13, %catch.dispatch18 ], [ %4, %lpad ]
  %exn.slot.1 = phi i8* [ %12, %catch.dispatch18 ], [ %3, %lpad ]
  %lpad.val = insertvalue { i8*, i32 } undef, i8* %exn.slot.1, 0
  %lpad.val37 = insertvalue { i8*, i32 } %lpad.val, i32 %ehselector.slot.1, 1
  resume { i8*, i32 } %lpad.val37
}

declare i32 @__gxx_personality_v0(...)

declare i32 @llvm.eh.typeid.for(i8*)

declare i8* @__cxa_begin_catch(i8*) local_unnamed_addr

declare i32 @printf(i8* nocapture readonly, ...)

declare void @__cxa_end_catch() local_unnamed_addr

define i32 @main() {
entry:
  %call = tail call i32 @foo(i32 3)
  %call1 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0), i32 %call)
  ret i32 0
}
