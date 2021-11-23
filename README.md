# py-c-preprocessor
A preprocessor for C files written in python

# What?
This is a preprocessor for C files written in python. It consumes C source and headers and generates outputs that are useful for code analysis and generation, such as:
 * Preprocessed C source
 * List of defined macros
 * Helpers for expanding and evaluating expresions

File can be passed in by paths, and headers included by #include directives, or passed in directly.

# Why?
I wanted to be able to parse C headers programmatically in python, and use found definitions for code generation. This originally started as just a way to gather all defined macros, but it turns out you need to do most other tasks to do that correctly in a realistic code base. All other features are an eventual result of this requirement.

I could have solved this by just using gcc -E, but I had some reasons not to:
 * Didnt want to write to the file system
 * Didnt want to introduce a compiler as a dependancy
 * Having all expressions in python makes code generation/examination very tidy

# How?
Mostly regex. Github Copilot was actually much more useful that expected.

# Usage

```python
from preprocessor import Preprocessor
p = Preprocessor()

# Include paths for headers can be specified
p.add_include_path('/path/to/headers')

# You can ignore missing includes to skip system included headers, such as stdio.h
p.ignore_missing_includes = True

# Macros can be defined before parsing
p.define('MACRO_A', '1')
p.define('MACRO_B', '(x + y / z)', ['x', 'y', 'z'])

# Source or headers can be included
p.include('/path/to/file.c')

# Multiple files can be included
# Source can also be supplied in place
p.include('/path', """
#ifdef MACRO_A
#define MACRO_C(x,y,z)  MACRO_B(x,y,z)
#else
#define MACRO_C(x,y,z)  (1)
#endif 

int main()
{
    return MACRO_C(1,2,3);
}
"""
)

# once the required source is parsed, expressions can be evaluated
print(p.evaluate('MACRO_A + MACRO_C(1,2,3)')) # returns 2

# The preprocessed source can then be evaluated
print(p.source())
# prints:
# int main()
# {
#     return 1 + 2 / 3;
# }
```


