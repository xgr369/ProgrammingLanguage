# ProgrammingLanguage

A programming language with Javascript syntax, intended to be embedded within applications.

Features: dynamically-typed variables, operations, multiple assignment, first-class functions (closures and C functions), if-else statements, while loop, and a C API.
Its development is not complete.

Example program (assuming `print` is implemented by the application):
```
import print;
var generator = function() {
    var x = 0;
    return function(y) {
        x = x + y;
        return x;
    };
};
var count = generator();
var a = count(6);
var b = count(7);
print(a);
export b;
```
