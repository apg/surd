Surd - An absurd Lisp interpreter
---------------------------------

This really doesn't do much but serve as an example of constructing a
very basic Lisp interpreter in C. It may, in the future serve as the 
basis for a larger project, but currently is absurd.

It supports abstraction, lists, symbols and fixnums. It does support
top level definitions with the `def` keyword, but at the moment doesn't
support "fancy forms" such as `let` and `letrec` nor does it support
macros. There's no first class `eval` or a way to report the current
lexical environment. But, everything *is* built out of conses and
cells, including closures. Closures are tagged as such, but stored
using the same cell fields that a cons uses.

In other words:

    c = cell()
    c.type = CLOSURE
    c.car = CODE
    c.cdr = ENV

Where as a cons:
   
    c = cell()
    c.type = CONS
    c.car = CAR
    c.cdr = CDR

Of course you can evaluate expressions:

    $ ./surd 
    surd 0> (((fn (y) (fn (x) (+ x y))) 1) 2)
    result 0: 3

The Allocator / GC
==================

surd uses libgc for now, though it did at one point use the one-pass 
mark-sweep as described by Armstong and Virding in "One Pass
Real-Time Generational Mark-Sweep Garbage Collection (1995)"
([link](http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.42.7791))


To do
=====

1. <strike>Generational collection -- this should be easy</strike>
2. Box top level definitions so that they can be replaced
3. First class `read`, `write`, `eval`.
4. Implement let, letrec (might require a special rec form)
5. tail-call optimization.
6. Larger standard library implemented in surd.
7. hygienic defmacros and quasiquote, unquote, unquote-splicing
8. ... 

