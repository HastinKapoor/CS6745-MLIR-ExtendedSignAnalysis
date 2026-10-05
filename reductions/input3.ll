; Interesting fact: dividing a value known to be nonnegative by a positive
; divisor leaves the result nonnegative.

define i32 @sdiv_nonnegative(i1 %flag, i32 %noise) {
entry:
  %unused = sub i32 %noise, 9
  br i1 %flag, label %one, label %zero

one:
  br label %join

zero:
  br label %join

join:
  %x = phi i32 [ 1, %one ], [ 0, %zero ]
  %q = sdiv i32 %x, 2
  ret i32 %q
}
