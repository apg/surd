Surd - An absurd Lisp interpreter
---------------------------------

Note: I don't know what I'm doing with this, but it's fun to hack on a bit.

## Memory management

Uses Boehm right now, but we'll see. Immediate fixnums and symbols. 

## Supported Language

### Types

*Fixnums* are 63-bit ints.

*Cons Pairs* are a `first` and a `rest` (`car` and `cdr`, traditionally).

*Closures* are applicables which contain a reference to the bindings that were present upon creation of the closure.

*Primitives* are applicables that are completely builtin to the system. Currently:

* `(cons V1 V2) -> C`
* `(first C) -> V`
* `(rest C) -> V|C`
* `(nth F C) -> V`
* `(cons? V) -> B`
* `(nil? V) -> B`
* `(eof? V) -> B`
* `(box? V) -> B`
* `(fixnum? V) -> B`
* `(symbol? V) -> B`
* `(string? V) -> B` 
* `(procedure? V) -> B`
* `(closure? V) -> B`
* `(primitive? V) -> B`
* `(foreign? V) -> B`
* `(+ F1 F2) -> F`
* `(- F1 [F2]) -> F`
* `(* F1 F2) -> F`
* `(/ F1 F2) -> F`
* `(% F1 F2) -> F`
* `(< F1 F2) -> F`
* `(= V1 V2) -> B`
* `(read P) -> V`
* `(write V P) -> V`
* `(open S S) -> P`
* `(close P) -> P`
* `(get-byte P) -> F`
* `(put-byte F P) -> F`
* `(put-str STR P) -> STR`
* `(load STR) -> V`
* `(strlen STR) -> F`
* `(box V) -> BX`
* `(unbox BX) -> V`
* `(set-box! BX V) -> BX`

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

