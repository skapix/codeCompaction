%"class.std::exception" = type { i32 (...)** }

$__clang_call_terminate = comdat any

@_ZTISt9exception = external constant i8*
@_ZTVSt9exception = external unnamed_addr constant { [5 x i8*] }, align 8

declare i8* @__cxa_allocate_exception(i64) local_unnamed_addr

declare void @_ZNSt9exceptionD1Ev(%"class.std::exception"*)

declare void @__cxa_throw(i8*, i8*, i8*)

define i32 @throws(i32 %a) {
entry:
  %cmp = icmp slt i32 %a, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %exception = tail call i8* @__cxa_allocate_exception(i64 8)
  %0 = bitcast i8* %exception to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTVSt9exception, i64 0, inrange i32 0, i64 2) to i32 (...)**), i32 (...)*** %0, align 8
  tail call void @__cxa_throw(i8* %exception, i8* bitcast (i8** @_ZTISt9exception to i8*), i8* bitcast (void (%"class.std::exception"*)* @_ZNSt9exceptionD1Ev to i8*))
  unreachable

if.end:
  ret i32 %a
}

define i32 @foo(i32 %a, i32 %b) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %mul = mul nsw i32 %a, %a
  %add = add nsw i32 %mul, %a
  %call = invoke i32 @throws(i32 %add)
          to label %try.cont unwind label %lpad

lpad:
  %0 = landingpad { i8*, i32 }
          cleanup
          catch i8* bitcast (i8** @_ZTISt9exception to i8*)
  %1 = extractvalue { i8*, i32 } %0, 0
  %2 = extractvalue { i8*, i32 } %0, 1
  %3 = tail call i32 @llvm.eh.typeid.for(i8* bitcast (i8** @_ZTISt9exception to i8*))
  %matches = icmp eq i32 %2, %3
  br i1 %matches, label %catch, label %ehcleanup

catch:
  %4 = tail call i8* @__cxa_begin_catch(i8* %1)
  %exn.byref = bitcast i8* %4 to %"class.std::exception"*
  %5 = bitcast i8* %4 to i8* (%"class.std::exception"*)***
  %vtable = load i8* (%"class.std::exception"*)**, i8* (%"class.std::exception"*)*** %5, align 8
  %vfn = getelementptr inbounds i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vtable, i64 2
  %6 = load i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vfn, align 8
  %call1 = tail call i8* %6(%"class.std::exception"* %exn.byref)
  invoke void @_Z8logErrorPKc(i8* %call1)
          to label %invoke.cont3 unwind label %lpad2

invoke.cont3:
  tail call void @exit(i32 -1)
  unreachable

lpad2:
  %7 = landingpad { i8*, i32 }
          cleanup
  %8 = extractvalue { i8*, i32 } %7, 0
  %9 = extractvalue { i8*, i32 } %7, 1
  invoke void @__cxa_end_catch()
          to label %ehcleanup unwind label %terminate.lpad

try.cont:
  %mul5 = mul nsw i32 %b, %b
  %add6 = add nsw i32 %mul5, %b
  %call9 = invoke i32 @throws(i32 %add6)
          to label %try.cont23 unwind label %lpad7

lpad7:
  %10 = landingpad { i8*, i32 }
          cleanup
          catch i8* bitcast (i8** @_ZTISt9exception to i8*)
  %11 = extractvalue { i8*, i32 } %10, 0
  %12 = extractvalue { i8*, i32 } %10, 1
  %13 = tail call i32 @llvm.eh.typeid.for(i8* bitcast (i8** @_ZTISt9exception to i8*))
  %matches12 = icmp eq i32 %12, %13
  br i1 %matches12, label %catch13, label %ehcleanup

catch13:
  %14 = tail call i8* @__cxa_begin_catch(i8* %11)
  %exn.byref16 = bitcast i8* %14 to %"class.std::exception"*
  %15 = bitcast i8* %14 to i8* (%"class.std::exception"*)***
  %vtable17 = load i8* (%"class.std::exception"*)**, i8* (%"class.std::exception"*)*** %15, align 8
  %vfn18 = getelementptr inbounds i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vtable17, i64 2
  %16 = load i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vfn18, align 8
  %call19 = tail call i8* %16(%"class.std::exception"* %exn.byref16)
  invoke void @_Z8logErrorPKc(i8* %call19)
          to label %invoke.cont21 unwind label %lpad20

invoke.cont21:
  tail call void @exit(i32 -1) 
  unreachable

lpad20:
  %17 = landingpad { i8*, i32 }
          cleanup
  %18 = extractvalue { i8*, i32 } %17, 0
  %19 = extractvalue { i8*, i32 } %17, 1
  invoke void @__cxa_end_catch()
          to label %ehcleanup unwind label %terminate.lpad

try.cont23:
  %add24 = add nsw i32 %add6, %add
  ret i32 %add24

ehcleanup:
  %ehselector.slot.0 = phi i32 [ %19, %lpad20 ], [ %12, %lpad7 ], [ %9, %lpad2 ], [ %2, %lpad ]
  %exn.slot.0 = phi i8* [ %18, %lpad20 ], [ %11, %lpad7 ], [ %8, %lpad2 ], [ %1, %lpad ]
  %lpad.val = insertvalue { i8*, i32 } undef, i8* %exn.slot.0, 0
  %lpad.val28 = insertvalue { i8*, i32 } %lpad.val, i32 %ehselector.slot.0, 1
  resume { i8*, i32 } %lpad.val28

terminate.lpad:
  %20 = landingpad { i8*, i32 }
          catch i8* null
  %21 = extractvalue { i8*, i32 } %20, 0
  tail call void @__clang_call_terminate(i8* %21)
  unreachable
}

declare i32 @__gxx_personality_v0(...)

declare i32 @llvm.eh.typeid.for(i8*)

declare i8* @__cxa_begin_catch(i8*) local_unnamed_addr

declare void @_Z8logErrorPKc(i8*) local_unnamed_addr

declare void @exit(i32) local_unnamed_addr

declare void @__cxa_end_catch() local_unnamed_addr

define linkonce_odr hidden void @__clang_call_terminate(i8*) local_unnamed_addr #5 comdat {
  %2 = tail call i8* @__cxa_begin_catch(i8* %0)
  tail call void @_ZSt9terminatev() #11
  unreachable
}

declare void @_ZSt9terminatev() local_unnamed_addr
