; ModuleID = 'main.cpp'
source_filename = "main.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%"class.std::exception" = type { i32 (...)** }

$__clang_call_terminate = comdat any

@_ZTISt9exception = external constant i8*
@_ZTVSt9exception = external unnamed_addr constant { [5 x i8*] }, align 8

; Function Attrs: optsize uwtable
define i32 @_Z12oppositeFunci(i32 %a) local_unnamed_addr #0 {
entry:
  %cmp = icmp sgt i32 %a, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %exception = tail call i8* @__cxa_allocate_exception(i64 8) #6
  %0 = bitcast i8* %exception to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTVSt9exception, i64 0, inrange i32 0, i64 2) to i32 (...)**), i32 (...)*** %0, align 8, !tbaa !1
  tail call void @__cxa_throw(i8* %exception, i8* bitcast (i8** @_ZTISt9exception to i8*), i8* bitcast (void (%"class.std::exception"*)* @_ZNSt9exceptionD1Ev to i8*)) #7
  unreachable

if.end:                                           ; preds = %entry
  %sub = sub nsw i32 0, %a
  %call = tail call i32 @_Z3bari(i32 %sub) #8
  ret i32 %call
}

declare i8* @__cxa_allocate_exception(i64) local_unnamed_addr

; Function Attrs: nounwind optsize
declare void @_ZNSt9exceptionD1Ev(%"class.std::exception"*) unnamed_addr #1

declare void @__cxa_throw(i8*, i8*, i8*) local_unnamed_addr

; Function Attrs: optsize
declare i32 @_Z3bari(i32) local_unnamed_addr #2

; Function Attrs: optsize uwtable
define i32 @_Z4funci(i32 %a) local_unnamed_addr #0 {
entry:
  %cmp = icmp slt i32 %a, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %exception = tail call i8* @__cxa_allocate_exception(i64 8) #6
  %0 = bitcast i8* %exception to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTVSt9exception, i64 0, inrange i32 0, i64 2) to i32 (...)**), i32 (...)*** %0, align 8, !tbaa !1
  tail call void @__cxa_throw(i8* %exception, i8* bitcast (i8** @_ZTISt9exception to i8*), i8* bitcast (void (%"class.std::exception"*)* @_ZNSt9exceptionD1Ev to i8*)) #7
  unreachable

if.end:                                           ; preds = %entry
  %call = tail call i32 @_Z3bari(i32 %a) #8
  ret i32 %call
}

; Function Attrs: optsize uwtable
define i32 @_Z3fooii(i32 %a, i32 %b) local_unnamed_addr #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %mul = mul nsw i32 %a, %a
  %add = add nsw i32 %mul, %a
  %call = invoke i32 @_Z4funci(i32 %add) #8
          to label %try.cont unwind label %lpad

lpad:                                             ; preds = %entry
  %0 = landingpad { i8*, i32 }
          cleanup
          catch i8* bitcast (i8** @_ZTISt9exception to i8*)
  %1 = extractvalue { i8*, i32 } %0, 0
  %2 = extractvalue { i8*, i32 } %0, 1
  %3 = tail call i32 @llvm.eh.typeid.for(i8* bitcast (i8** @_ZTISt9exception to i8*)) #6
  %matches = icmp eq i32 %2, %3
  br i1 %matches, label %catch, label %ehcleanup

catch:                                            ; preds = %lpad
  %4 = tail call i8* @__cxa_begin_catch(i8* %1) #6
  %exn.byref = bitcast i8* %4 to %"class.std::exception"*
  %5 = bitcast i8* %4 to i8* (%"class.std::exception"*)***
  %vtable = load i8* (%"class.std::exception"*)**, i8* (%"class.std::exception"*)*** %5, align 8, !tbaa !1
  %vfn = getelementptr inbounds i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vtable, i64 2
  %6 = load i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vfn, align 8
  %call1 = tail call i8* %6(%"class.std::exception"* %exn.byref) #9
  invoke void @_Z8logErrorPKc(i8* %call1) #8
          to label %invoke.cont3 unwind label %lpad2

invoke.cont3:                                     ; preds = %catch
  tail call void @exit(i32 -1) #10
  unreachable

lpad2:                                            ; preds = %catch
  %7 = landingpad { i8*, i32 }
          cleanup
  %8 = extractvalue { i8*, i32 } %7, 0
  %9 = extractvalue { i8*, i32 } %7, 1
  invoke void @__cxa_end_catch()
          to label %ehcleanup unwind label %terminate.lpad

try.cont:                                         ; preds = %entry
  %mul5 = mul nsw i32 %b, %b
  %add6 = add nsw i32 %mul5, %b
  %call9 = invoke i32 @_Z4funci(i32 %add6) #8
          to label %try.cont23 unwind label %lpad7

lpad7:                                            ; preds = %try.cont
  %10 = landingpad { i8*, i32 }
          cleanup
          catch i8* bitcast (i8** @_ZTISt9exception to i8*)
  %11 = extractvalue { i8*, i32 } %10, 0
  %12 = extractvalue { i8*, i32 } %10, 1
  %13 = tail call i32 @llvm.eh.typeid.for(i8* bitcast (i8** @_ZTISt9exception to i8*)) #6
  %matches12 = icmp eq i32 %12, %13
  br i1 %matches12, label %catch13, label %ehcleanup

catch13:                                          ; preds = %lpad7
  %14 = tail call i8* @__cxa_begin_catch(i8* %11) #6
  %exn.byref16 = bitcast i8* %14 to %"class.std::exception"*
  %15 = bitcast i8* %14 to i8* (%"class.std::exception"*)***
  %vtable17 = load i8* (%"class.std::exception"*)**, i8* (%"class.std::exception"*)*** %15, align 8, !tbaa !1
  %vfn18 = getelementptr inbounds i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vtable17, i64 2
  %16 = load i8* (%"class.std::exception"*)*, i8* (%"class.std::exception"*)** %vfn18, align 8
  %call19 = tail call i8* %16(%"class.std::exception"* %exn.byref16) #9
  invoke void @_Z8logErrorPKc(i8* %call19) #8
          to label %invoke.cont21 unwind label %lpad20

invoke.cont21:                                    ; preds = %catch13
  tail call void @exit(i32 -1) #10
  unreachable

lpad20:                                           ; preds = %catch13
  %17 = landingpad { i8*, i32 }
          cleanup
  %18 = extractvalue { i8*, i32 } %17, 0
  %19 = extractvalue { i8*, i32 } %17, 1
  invoke void @__cxa_end_catch()
          to label %ehcleanup unwind label %terminate.lpad

try.cont23:                                       ; preds = %try.cont
  %add24 = add nsw i32 %add6, %add
  ret i32 %add24

ehcleanup:                                        ; preds = %lpad20, %lpad2, %lpad7, %lpad
  %ehselector.slot.0 = phi i32 [ %19, %lpad20 ], [ %12, %lpad7 ], [ %9, %lpad2 ], [ %2, %lpad ]
  %exn.slot.0 = phi i8* [ %18, %lpad20 ], [ %11, %lpad7 ], [ %8, %lpad2 ], [ %1, %lpad ]
  %lpad.val = insertvalue { i8*, i32 } undef, i8* %exn.slot.0, 0
  %lpad.val28 = insertvalue { i8*, i32 } %lpad.val, i32 %ehselector.slot.0, 1
  resume { i8*, i32 } %lpad.val28

terminate.lpad:                                   ; preds = %lpad20, %lpad2
  %20 = landingpad { i8*, i32 }
          catch i8* null
  %21 = extractvalue { i8*, i32 } %20, 0
  tail call void @__clang_call_terminate(i8* %21) #11
  unreachable
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind readnone
declare i32 @llvm.eh.typeid.for(i8*) #3

declare i8* @__cxa_begin_catch(i8*) local_unnamed_addr

; Function Attrs: optsize
declare void @_Z8logErrorPKc(i8*) local_unnamed_addr #2

; Function Attrs: noreturn nounwind optsize
declare void @exit(i32) local_unnamed_addr #4

declare void @__cxa_end_catch() local_unnamed_addr

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8*) local_unnamed_addr #5 comdat {
  %2 = tail call i8* @__cxa_begin_catch(i8* %0) #6
  tail call void @_ZSt9terminatev() #11
  unreachable
}

declare void @_ZSt9terminatev() local_unnamed_addr

attributes #0 = { optsize uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind optsize "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { optsize "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind readnone }
attributes #4 = { noreturn nounwind optsize "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { noinline noreturn nounwind }
attributes #6 = { nounwind }
attributes #7 = { noreturn }
attributes #8 = { optsize }
attributes #9 = { nounwind optsize }
attributes #10 = { noreturn nounwind optsize }
attributes #11 = { noreturn nounwind }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.0 (trunk 290798)"}
!1 = !{!2, !2, i64 0}
!2 = !{!"vtable pointer", !3, i64 0}
!3 = !{!"Simple C++ TBAA"}
