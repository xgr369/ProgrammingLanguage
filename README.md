# ProgrammingLanguage

"Programming Language" (no name decided yet) is a Javascript dialect focused on embedability.

Its main features are multiple assignment, if-else statements, while loops, first-class functions, and a C API.

# Installation

1. Clone this repository.
2. Compile with a C compiler.

# Features
## Variable declaration and assignment
```javascript
var a = 0;
var b = 1;
a, b = b, a;
```
Functions can also return multiple values.
```javascript
var a, b = function() { return 1, 2; }();
```
## If-else statements
```javascript
var c = null;
if (a > 0) {
    c = "positive";
} elseif (a == 0) {
    c = "zero";
} else {
    c = "negative";
}
```
## While loops
```javascript
while (a < 5) {
    a = a + 1;
}
```
## Functions
```javascript
import print, input;
var prompt = function() {
    print("Hello, " + input());
};
```
## Export values
The script can export values that are then accessed, in `L->registry`, by the application.
```javascript
var x = input();
export x;
```
However, this doesn't work properly with functions defined in the script, as they are discarded upon the script's termination.
## Missing features
The language lacks the following features, however: comments, tables/arrays, corountines/async, object-oriented programming, "break" statements, "goto" statements, empty return statements.

# Sample program
Assuming that `print` is implemented by the host application:
```javascript
import print;
var generatePrintFunction = function(name) {
    return function() {
        print(name);
    };
};
var print_john = generatePrintFunction("John Smith");
print_john();
var print_jane = generatePrintFunction("Jane Smith");
print_jane();
```
