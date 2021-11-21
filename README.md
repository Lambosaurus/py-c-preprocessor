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

# How?
Mostly regex. Github Copilot was actually much more useful that expected.

# Features:
 * Handles multi line directives
 * Removes comments
 * #ifdef and other conditional directives
 * #include directives
 * #define directives
 * Macro expansion
