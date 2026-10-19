module {
  llvm.mlir.global private constant @__extended_sign_filename_3("dynamic-checking/overflow-bug.mlir\00") {addr_space = 0 : i32}
  llvm.mlir.global private constant @__extended_sign_filename_2("dynamic-checking/overflow-bug.mlir\00") {addr_space = 0 : i32}
  llvm.mlir.global private constant @__extended_sign_filename_1("dynamic-checking/overflow-bug.mlir\00") {addr_space = 0 : i32}
  llvm.func @__extended_sign_assert(i1, i32, i64, !llvm.ptr, i32, i32)
  llvm.func @main() -> i32 {
    %0 = llvm.mlir.constant(2147483647 : i32) : i32
    %1 = llvm.mlir.constant(0 : i32) : i32
    %2 = llvm.icmp "sgt" %0, %1 : i32
    %3 = llvm.mlir.addressof @__extended_sign_filename_1 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %5 = llvm.mlir.constant(4 : i32) : i32
    %6 = llvm.mlir.constant(1 : i64) : i64
    %7 = llvm.mlir.constant(3 : i32) : i32
    %8 = llvm.mlir.constant(12 : i32) : i32
    llvm.call @__extended_sign_assert(%2, %5, %6, %4, %7, %8) : (i1, i32, i64, !llvm.ptr, i32, i32) -> ()
    %9 = llvm.add %0, %0 : i32
    %10 = llvm.mlir.constant(0 : i32) : i32
    %11 = llvm.icmp "sgt" %9, %10 : i32
    %12 = llvm.mlir.addressof @__extended_sign_filename_2 : !llvm.ptr
    %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %14 = llvm.mlir.constant(4 : i32) : i32
    %15 = llvm.mlir.constant(2 : i64) : i64
    %16 = llvm.mlir.constant(4 : i32) : i32
    %17 = llvm.mlir.constant(12 : i32) : i32
    llvm.call @__extended_sign_assert(%11, %14, %15, %13, %16, %17) : (i1, i32, i64, !llvm.ptr, i32, i32) -> ()
    %18 = llvm.mlir.constant(0 : i32) : i32
    %19 = llvm.mlir.constant(0 : i32) : i32
    %20 = llvm.icmp "eq" %18, %19 : i32
    %21 = llvm.mlir.addressof @__extended_sign_filename_3 : !llvm.ptr
    %22 = llvm.getelementptr %21[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %23 = llvm.mlir.constant(3 : i32) : i32
    %24 = llvm.mlir.constant(3 : i64) : i64
    %25 = llvm.mlir.constant(5 : i32) : i32
    %26 = llvm.mlir.constant(13 : i32) : i32
    llvm.call @__extended_sign_assert(%20, %23, %24, %22, %25, %26) : (i1, i32, i64, !llvm.ptr, i32, i32) -> ()
    llvm.return %18 : i32
  }
}

