; Interesting fact: ExtendedSign can join constants from two control-flow
; predecessors and prove the merged value is nonnegative.

define i32 @join_nonnegative(i1 %flag, i32 %noise) {
entry:
  %unused = add i32 %noise, 123
  br i1 %flag, label %positive, label %zero

positive:
  br label %join

zero:
  br label %join

join:
  %x = phi i32 [ 21, %positive ], [ 0, %zero ]
  ret i32 %x
}
