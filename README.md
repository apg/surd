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
    surd 0> (((lambda (y) (lambda (x) (+ x y))) 1) 2)
    result 0: 3

By default the heap size is 1000 cells. You'll never need more than
that because you're unlikely to right a real program in it!

The Allocator / GC
==================

Yes, surd is absurd, but it *does* have a garbage collector. The
algorithm is a one-pass mark-sweep as described by Armstong and
Virding in "One Pass Real-Time Generational Mark-Sweep Garbage
Collection (1995)"
([link](http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.42.7791))

I haven't yet modified it to make it generational, or incremental
even, but those are fairly trivial changes, and I'll do so at some
point.

The allocator is dirt simple since cells are the same size. Cells
are bump allocated off of a `malloc`'d array of cells when the free-list
is empty. The free list is filled lazily by the garbage collector. 

To do
=====

1. Generational collection -- this should be easy
2. Box top level definitions so that they can be replaced
3. First class `read`, `write`, `eval`.

   If I had `eval`, I could redefine it and add basic support for
   macros. 

4. Surd in surd?
5. Implement let, let*
6. tail-call optimization?
7. Larger standard library implemented in surd.
8. Grow the heap if GC fails to recovery any cells
9. ... 

