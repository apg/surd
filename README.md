Surd - An absurd Lisp interpreter
---------------------------------

Note: I don't know what I'm doing with this, but it's fun to hack on a bit.

## Memory management

Uses Boehm.

## Features

## Supported Language

### Types

*Fixnums* are whatever C `int` is on your system.

*Cons Pairs* are a `first` and a `rest` (`car` and `cdr`, traditionally).

*Closures* are applicables which contain a reference to the bindings that were present upon creation of the closure.

*Primitives* are applicables that are completely builtin to the system. Currently:

`cons`, `first`, `rest`, `nth`, `cons?`, `nil?`, `eof?`, `fixnum?`, `symbol?`, `closure?`, `procedure?`, `foreign?`, `+`, `-`, `*`, `/`, `%`, `<`, `=`... 

*Symbols* as in the traditional Lisp point of view.

There are no boolean values. To represent "truth" we use a symbol named `true`. We use `nil` as False, presently, which I'm guessing we'll find to be a horrible idea.

### Special forms

`(def name value)` defines `name` in the top level environment, and assigns it to `value`.

`(if x y z)` if `x` is true, run `y`, else run `z`.

`(lam (x ...) body)` defines a closure that captures the current environment.

`(quote x)` defines a quotation which evaluates to `x`


## Error Handling

Does not exist yet.

## Usage

You can interact with the core data structures. See surd.h.

