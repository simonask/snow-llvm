# Snow TODO

## Runtime

### VM

* Multithreaded Garbage Collection.
* Background JIT compilation (replaces LLVM lazy compilation).
* Call-site inline cache VM optimization.
* More flexible Type system, suitable for inline cache optimizations.
* Fix the numerous bugs in the handling of named arguments.
* Consider supporting multiple code paths for specific type combinations.
* Exception objects should generate a suitable backtrace.

### Parser

* Implement a short-hand syntax for creating maps.
* Implement type hints for type inference.

## Standard Library

### Object

* Implement `.is_a?`.

### Array

* Automatic resize should make better guesses.

### Map

* Decide on better heuristics for switching between flat and hash-based maps.
* Use FlatMap<K,V> instead of the current C-style implementation.

### AST

* Runtime AST introspection and manipulation.

### Integer/Float

* Provide separate class objects for Integer and Float.
* Implement a BigInteger class.