#ifndef DELAY_H
#define DELAY_H
#include <utility>
#include <memory>
#include <iostream>
#include <intrin.h>

template <class T>
constexpr void test_helper(T&&) {}

#define IS_CONSTEXPR(...) noexcept(test_helper(__VA_ARGS__))

template <int a> struct Nop {
      static void nop() {
      __nop();
      Nop<a-1>::nop();
    }
};
template <> struct Nop<0> {
    static void nop() {}
};

void bar(int x) {
    for (int i = 0; i < x; i++)
      Nop<1>::nop();
}

template <bool e, int T> struct CTorRT
{
  CTorRT(int) {
    if (T == 0) return;
    if (T >= 40) bar(T - 10);
    else Nop<T>::nop();
  }
};

template <int T> struct CTorRT<false, T>
{
    CTorRT(int v) { bar(v); }
};

#define DELAY_CYCLE(X) { CTorRT<IS_CONSTEXPR(X), IS_CONSTEXPR(X) ? X : 0> a(X); }

#endif // DELAY_H
