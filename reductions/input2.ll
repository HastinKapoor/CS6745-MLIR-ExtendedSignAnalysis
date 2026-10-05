; Interesting fact: a signed max with one positive operand is positive even
; when the other operand is unknown.

declare i32 @llvm.smax.i32(i32, i32)

define i32 @signed_smax_positive(i32 %x, i32 %noise) {
entry:
  %unused = mul i32 %noise, -7
  %r = call i32 @llvm.smax.i32(i32 2, i32 %x)
  ret i32 %r
}
