// RUN: %clang_cc1 %s -fsyntax-only -verify
struct A {
  A() { f(); } // expected-warning {{call to pure virtual member function 'f'; overrides of 'f' in subclasses are not available in the constructor of 'A'}}
  ~A() { f(); } // expected-warning {{call to pure virtual member function 'f'; overrides of 'f' in subclasses are not available in the destructor of 'A'}}

  virtual void f() = 0; // expected-note 2 {{'f' declared here}}
};
