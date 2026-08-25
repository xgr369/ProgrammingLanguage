# ProgrammingLanguage

"Programming Language" (no name decided yet) is a Javascript dialect focused on embedability.

Its features include multiple assignment, if-else statements, while loops, and first-class functions. Compared to Lua and Javascript, it lacks the following features: tables/arrays, corountines/async, object-oriented programming, "break" statements, "goto" statements, empty return statements.


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
