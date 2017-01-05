; ModuleID = 'main.cpp'
source_filename = "main.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

$_ZSt4sqrtIiEN9__gnu_cxx11__enable_ifIXsr12__is_integerIT_EE7__valueEdE6__typeES2_ = comdat any

@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; Function Attrs: uwtable
define i32 @_Z5func1i(i32 %i) #0 {
entry:
  %i.addr = alloca i32, align 4
  %l = alloca i32, align 4
  %k = alloca i32, align 4
  %k2 = alloca i32, align 4
  store i32 %i, i32* %i.addr, align 4
  store i32 1, i32* %l, align 4
  %0 = load i32, i32* %i.addr, align 4
  %cmp = icmp slt i32 %0, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %1 = load i32, i32* %i.addr, align 4
  %2 = load i32, i32* %i.addr, align 4
  %mul = mul nsw i32 %1, %2
  store i32 %mul, i32* %k, align 4
  %3 = load i32, i32* %i.addr, align 4
  %4 = load i32, i32* %k, align 4
  %add = add nsw i32 %3, %4
  %5 = load i32, i32* %l, align 4
  %add1 = add nsw i32 %5, %add
  store i32 %add1, i32* %l, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  %6 = load i32, i32* %i.addr, align 4
  %call = call double @_ZSt4sqrtIiEN9__gnu_cxx11__enable_ifIXsr12__is_integerIT_EE7__valueEdE6__typeES2_(i32 %6)
  %conv = fptosi double %call to i32
  store i32 %conv, i32* %k2, align 4
  %7 = load i32, i32* %i.addr, align 4
  %8 = load i32, i32* %k2, align 4
  %add3 = add nsw i32 %7, %8
  %9 = load i32, i32* %l, align 4
  %add4 = add nsw i32 %9, %add3
  store i32 %add4, i32* %l, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %10 = load i32, i32* %l, align 4
  ret i32 %10
}

; Function Attrs: inlinehint nounwind uwtable
define linkonce_odr double @_ZSt4sqrtIiEN9__gnu_cxx11__enable_ifIXsr12__is_integerIT_EE7__valueEdE6__typeES2_(i32 %__x) #1 comdat {
entry:
  %__x.addr = alloca i32, align 4
  store i32 %__x, i32* %__x.addr, align 4
  %0 = load i32, i32* %__x.addr, align 4
  %conv = sitofp i32 %0 to double
  %call = call double @sqrt(double %conv) #5
  ret double %call
}

; Function Attrs: uwtable
define i32 @_Z5func2i(i32 %i) #0 {
entry:
  %i.addr = alloca i32, align 4
  %l = alloca i32, align 4
  %k = alloca i32, align 4
  %k2 = alloca i32, align 4
  store i32 %i, i32* %i.addr, align 4
  store i32 1, i32* %l, align 4
  %0 = load i32, i32* %i.addr, align 4
  %cmp = icmp slt i32 %0, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %1 = load i32, i32* %i.addr, align 4
  %2 = load i32, i32* %i.addr, align 4
  %mul = mul nsw i32 %1, %2
  store i32 %mul, i32* %k, align 4
  %3 = load i32, i32* %i.addr, align 4
  %4 = load i32, i32* %k, align 4
  %add = add nsw i32 %3, %4
  %5 = load i32, i32* %l, align 4
  %add1 = add nsw i32 %5, %add
  store i32 %add1, i32* %l, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  %6 = load i32, i32* %i.addr, align 4
  %call = call double @_ZSt4sqrtIiEN9__gnu_cxx11__enable_ifIXsr12__is_integerIT_EE7__valueEdE6__typeES2_(i32 %6)
  %conv = fptosi double %call to i32
  store i32 %conv, i32* %k2, align 4
  %7 = load i32, i32* %i.addr, align 4
  %8 = load i32, i32* %k2, align 4
  %add3 = add nsw i32 %7, %8
  %9 = load i32, i32* %l, align 4
  %add4 = add nsw i32 %9, %add3
  store i32 %add4, i32* %l, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %10 = load i32, i32* %l, align 4
  ret i32 %10
}

; Function Attrs: norecurse uwtable
define i32 @main() #2 {
entry:
  %retval = alloca i32, align 4
  %param = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %call = call i32 (i8*, ...) @scanf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i32 0, i32 0), i32* %param)
  %0 = load i32, i32* %param, align 4
  %call1 = call i32 @_Z5func1i(i32 %0)
  store i32 %call1, i32* %i, align 4
  %1 = load i32, i32* %param, align 4
  %add = add nsw i32 %1, 1
  %call2 = call i32 @_Z5func2i(i32 %add)
  %2 = load i32, i32* %i, align 4
  %add3 = add nsw i32 %2, %call2
  store i32 %add3, i32* %i, align 4
  %3 = load i32, i32* %i, align 4
  %call4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i32 0, i32 0), i32 %3)
  ret i32 0
}

declare i32 @scanf(i8*, ...) #3

declare i32 @printf(i8*, ...) #3

; Function Attrs: nounwind readnone
declare double @sqrt(double) #4

attributes #0 = { uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { inlinehint nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { norecurse uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nounwind readnone "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind readnone }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.0 (trunk 285500)"}
