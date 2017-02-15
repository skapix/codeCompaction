define i32 @foo(i32 %i1) {
entry:
  %cmp1 = icmp sgt i32 %i1, 0
  br i1 %cmp1, label %greater.zero, label %less.zero

greater.zero:
  %g.a = add nsw i32 %i1, 32
  %g.b = sub nsw i32 %i1, 31
  br label %body
less.zero:
  %l.a = add nsw i32 %i1, 31
  %l.b = sub nsw i32 %i1, 32
  br label %body

body:
  %phi1 = phi i32 [ %g.a, %greater.zero ], [ %l.a, %less.zero ]
  %phi2 = phi i32 [ %g.b, %greater.zero ], [ %l.b, %less.zero ]
  %someCalc1 = add nsw i32 %phi1, %phi2
  %someCalc2 = mul nsw i32 %i1, %phi1
  %someCalc3 = mul nsw i32 %i1, %phi2
  %someCalc4 = sub nsw i32 %someCalc2, %someCalc3
  %someCalc5 = add nsw i32 %someCalc4, %someCalc1
  ret i32 %someCalc5
}

define i32 @bar(i32 %i0) {
entry:
  %cmp1 = icmp sge i32 %i0, 0
  br i1 %cmp1, label %greater.zero, label %less.zero

greater.zero:
  %g.a = add nsw i32 %i0, 56
  %g.b = sub nsw i32 %i0, 30
  br label %body
less.zero:
  %l.a = add nsw i32 %i0, 27
  %l.b = sub nsw i32 %i0, 8
  br label %body

body:
  %phi1 = phi i32 [ %g.a, %greater.zero ], [ %l.a, %less.zero ]
  %phi2 = phi i32 [ %g.b, %greater.zero ], [ %l.b, %less.zero ]
  %someCalc1 = add nsw i32 %phi1, %phi2
  %someCalc2 = mul nsw i32 %i0, %phi1
  %someCalc3 = mul nsw i32 %i0, %phi2
  %someCalc4 = sub nsw i32 %someCalc2, %someCalc3
  %someCalc5 = add nsw i32 %someCalc4, %someCalc1
  ret i32 %someCalc5
}



define i32 @main() {
entry:
  ret i32 0
}
