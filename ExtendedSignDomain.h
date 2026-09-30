//===- ExtendedSignDomain.h - The abstract domain -------------------------===//
//
// An eight-point lattice recording signed information about an integer.
// ExtendedSignAnalysis leaves i1 boolean results unknown, because the bit
// pattern 1 is true for control flow but -1 as a signed one-bit integer.
//
//              Top
//         /     |     \
//   Nonpositive | Nonnegative
//     /       \ | /       \
// Negative     Zero     Positive
//     \         |          /
//      \        |         One
//       \       |        /
//             Bottom
//
// MLIR's dataflow framework asks only three things of a lattice value:
//
//   * a default constructor, which must produce the bottom element, because the
//     solver starts every value optimistically and lowers it as facts arrive;
//   * a static join(), which must be commutative, associative, idempotent, and
//     monotone -- assertions in Lattice<> check monotonicity in debug builds;
//   * operator== and print().
//
//===----------------------------------------------------------------------===//

#ifndef EXTENDED_SIGN_DOMAIN_H
#define EXTENDED_SIGN_DOMAIN_H

#include "llvm/Support/raw_ostream.h"

namespace extended_sign {

enum class Kind {
  Bottom,
  One,
  Negative,
  Zero,
  Positive,
  Nonnegative,
  Nonpositive,
  Top
};

inline const char *name(Kind kind) {
  switch (kind) {
  case Kind::Bottom:
    return "bottom";
  case Kind::One:
    return "one";
  case Kind::Negative:
    return "negative";
  case Kind::Zero:
    return "zero";
  case Kind::Positive:
    return "positive";
  case Kind::Nonnegative:
    return "nonnegative";
  case Kind::Nonpositive:
    return "nonpositive";
  case Kind::Top:
    return "top";
  }
  return "top";
}

struct ExtendedSignState {
  Kind kind = Kind::Bottom;

  ExtendedSignState() = default;
  /* implicit */ ExtendedSignState(Kind kind) : kind(kind) {}

  static ExtendedSignState bottom() { return Kind::Bottom; }
  static ExtendedSignState top() { return Kind::Top; }

  bool isBottom() const { return kind == Kind::Bottom; }

  /// Least upper bound in the extended-sign lattice.
  static ExtendedSignState join(const ExtendedSignState &lhs,
                                const ExtendedSignState &rhs) {
    if (lhs.kind == Kind::Bottom)
      return rhs;
    if (rhs.kind == Kind::Bottom)
      return lhs;
    if (lhs.kind == rhs.kind)
      return lhs;
    if (lhs.kind == Kind::Top || rhs.kind == Kind::Top)
      return top();

    if (lhs.kind == Kind::Nonnegative || rhs.kind == Kind::Nonnegative) {
      Kind other = lhs.kind == Kind::Nonnegative ? rhs.kind : lhs.kind;
      if (other == Kind::One || other == Kind::Zero ||
          other == Kind::Positive)
        return Kind::Nonnegative;
      return top();
    }

    if (lhs.kind == Kind::Nonpositive || rhs.kind == Kind::Nonpositive) {
      Kind other = lhs.kind == Kind::Nonpositive ? rhs.kind : lhs.kind;
      if (other == Kind::Negative || other == Kind::Zero)
        return Kind::Nonpositive;
      return top();
    }

    if ((lhs.kind == Kind::One && rhs.kind == Kind::Positive) ||
        (lhs.kind == Kind::Positive && rhs.kind == Kind::One))
      return Kind::Positive;

    if ((lhs.kind == Kind::One && rhs.kind == Kind::Zero) ||
        (lhs.kind == Kind::Zero && rhs.kind == Kind::One) ||
        (lhs.kind == Kind::Positive && rhs.kind == Kind::Zero) ||
        (lhs.kind == Kind::Zero && rhs.kind == Kind::Positive))
      return Kind::Nonnegative;

    if ((lhs.kind == Kind::Negative && rhs.kind == Kind::Zero) ||
        (lhs.kind == Kind::Zero && rhs.kind == Kind::Negative))
      return Kind::Nonpositive;

    return top();
  }

  bool operator==(const ExtendedSignState &other) const { return kind == other.kind; }
  bool operator!=(const ExtendedSignState &other) const { return kind != other.kind;}

  void print(llvm::raw_ostream &os) const { os << name(kind); }
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const ExtendedSignState &state) {
  state.print(os);
  return os;
}

} // namespace extended_sign

#endif
