module {
  llvm.func @main() -> i32 {
    %max = llvm.mlir.constant(2147483647 : i32) : i32
    %sum = llvm.add %max, %max : i32
    %zero = llvm.mlir.constant(0 : i32) : i32
    llvm.return %zero : i32
  }
}
