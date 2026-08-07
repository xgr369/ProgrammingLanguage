# ProgrammingLanguage

A dynamically-typed language based on Lua, but with Javascript syntax and a number of other differences.

Features: stack-based VM, variables, operations, multiple assignment, first-class functions (closures and C functions), if-else statements, and a C API.
Its development is not complete.

Example program:
```
generate_counter = function() {
    x = 0;
    return function(y) {
        x = x + y;
        return x;
    };
};
count = generate_counter();
h = count(9);
g = count(9);
```
