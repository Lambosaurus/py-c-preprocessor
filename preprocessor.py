
import re
import os.path

class PreprocessorRule():
    def __init__(self, pattern, action, always_parse = False):
        self.pattern = re.compile(pattern)
        self.action = action
        self.always_parse = always_parse

    def invoke(self, line, parse_enabled):
        if parse_enabled or self.always_parse:
            match = self.pattern.match(line)
            if match:
                self.action(match.groups())
                return True
        return False


IF_STATE_NOW  = 0
IF_STATE_SEEK = 1
IF_STATE_SKIP = 2

class Preprocessor():

    #
    #      CONSTRUCTOR / DESTRUCTOR
    #

    def __init__(self):
        self.macros = {}
        self.varidic_macros = {}

        self.rules = [
            # Conditional tokens
            PreprocessorRule(r"^#if\s+([^\r\n]*)", self._rule_if, True),
            PreprocessorRule(r"^#ifdef\s+(\w+)", self._rule_ifdef, True),
            PreprocessorRule(r"^#ifndef\s+(\w+)", self._rule_ifndef, True),
            PreprocessorRule(r"^#elif\s+([^\r\n]*)", self._rule_elif, True),
            PreprocessorRule(r"^#endif", self._rule_endif, True),
            PreprocessorRule(r"^#else", self._rule_else, True),


            # Standalone tokens
            PreprocessorRule(r"^#pragma\s+([^\r\n]*)", self._rule_pragma),
            PreprocessorRule(r"^#error\s+([^\r\n]*)", self._rule_error),
            PreprocessorRule(r"^#include\s+\"([^\"]*)\"", self._rule_include),
            PreprocessorRule(r"^#include\s+<([^>]*)>", self._rule_include),
            PreprocessorRule(r"^#undef\s+(\w+)", self._rule_undef),
            
            # Define statements. Order is important.
            PreprocessorRule(r"^#define\s+(\w+)\s*\(([^\)]*)\)\s+([^\r\n]*)", self._rule_define),
            PreprocessorRule(r"^#define\s+(\w+)\s+([^\r\n]*)", self._rule_define),
            PreprocessorRule(r"^#define\s+(\w+)", self._rule_define),
        ]

        self._content_enabled = IF_STATE_NOW
        self._enable_stack = []

        self.source_lines = []

        self.include_rule = lambda name: True
        self.include_paths = []
        self.ignore_missing_includes = False

        self._local_path = ""

    #
    #      PUBLIC INTERFACE
    #

    # adds an include path for looking up include files
    # path may also be an enumerable containing multiple paths.
    def add_include_path(self, *paths):
        for path in paths:
            self._add_include_path(path)

    # returns the output source file as a string
    def source(self):
        return "\n".join(self.source_lines)

    # Defines a symbol
    def define(self, key, value = "", args = None):
        if args:
            self.varidic_macros[key] = (args, value)
        else:
            self.macros[key] = value
    
    # Undefines a symbol
    def undefine(self, key):
        if key in self.macros:
            del self.macros[key]

    # returns true if a preprocessor symbol is defined
    def is_defined(self, key):
        return key in self.macros

    def evaluate(self, expr):
        return self._evaluate_expression(expr)

    # Consumes a file and preprocesses it.
    def include(self, path, may_ignore = False):
        path = self._resolve_path(path)

        if not os.path.exists(path):
            if may_ignore:
                return
            else:
                raise Exception("file \"{}\" cannot be found".format(path))
        
        # Update the new local path to be relative to the current path.
        prior_local = self._set_local_path(path)
        stack_depth = len(self._enable_stack)

        prior_line = None
        in_comment = False

        # open the file and start 
        with open(path, 'r') as file:
            for line in file.readlines():
                # do the actual parsing
                line, prior_line = self._join_escaped_line(line, prior_line)
                if line:
                    line, in_comment = self._strip_comments(line, in_comment)
                    self._preprocess_line(line)
                    
 
        if len(self._enable_stack) != stack_depth:
            raise Exception("unterminated #if found")
        if in_comment:
            raise Exception("unterminated comment found")

        self._restore_local_path(prior_local)

    #
    #     LINE PARSING
    #

    # Lines ending with '\' need to be joined.
    def _join_escaped_line(self, line, prior):
        if prior:
            line = prior + line

        if line.endswith('\\\n'):
            return None, line[:-2] # make sure to discard the '\'
        else:
            return line, None

    # Removes any comments.
    # /**/ comment block state is handled over multiple lines with in_comment variable.
    def _strip_comments(self, line, in_comment):
        # first, everything after // is lost
        if "//" in line:
            line = line.split("//", 1)[0]

        if in_comment:
            line, comment = "", line

        while True:
            # Toggle between checking for start and end of comments until no more are found.
            if in_comment:
                if "*/" in comment:
                    # add the components after the comment ends
                    line += comment.split("*/", 1)[1]
                    in_comment = False
                else:
                    break
            else: # not in comment
                if "/*" in line:
                    # grab everything before the comment starts
                    line, comment = line.split("/*", 1)
                    in_comment = True
                else:
                    break

        return line, in_comment

    # Runs a line through the preprocessor
    def _preprocess_line(self, line):
        line = line.strip()
        if line:
            enabled = self._flow_enabled()
            for rule in self.rules:
                if rule.invoke(line, enabled):
                    break
            else:
                if enabled:
                    # Didnt match a preprocessor line - its source.
                    line = self._expand_macros(line)
                    self.source_lines.append(line)

    #
    #     PATH RESOLUTION
    #

    # Resolves an include path to the current working directory.
    def _resolve_path(self, path):
        # try local path first
        candiate = os.path.normpath(os.path.join(self._local_path, path))
        if os.path.exists(candiate):
            return candiate

        # test all include paths
        for dir in self.include_paths:
            candiate = os.path.normpath(os.path.join(dir, path))
            if os.path.exists(candiate):
                return candiate
        
        return path # just return the path as a last resort.

    # Sets the current local path to the directory of the current processed file
    # Returns the previous path so that it may be restored
    def _set_local_path(self, path):
        prior = self._local_path
        self._local_path = os.path.dirname(path)
        return prior

    # Restores the local path to the previous value
    def _restore_local_path(self, prior):
        self._local_path = prior

    def _add_include_path(self, path):
        self.include_paths.append(os.path.normpath(path))

    #
    #      PREPROCESSOR DIRECTIVE RULES
    #

    # Rule to handle: #define <token> OR #define <token> <any> OR  #define <token>(<any>) <any>
    def _rule_define(self, args):
        if len(args) == 1:
            self.define(args[0])
        elif len(args) == 2:
            self.define(args[0], args[1])
        elif len(args) == 3:
            varargs = [ a.strip() for a in args[1].split(",") ]
            self.define(args[0], args[2], varargs)

    # Rule to handle: #if <expression>
    def _rule_if(self, args):
        self._flow_enter_if(self._test_expression(args[0]))

    # Rule to handle: #ifdef <token>
    def _rule_ifdef(self, args):
        self._flow_enter_if(self.is_defined(args[0]))

    # Rule to handle: #ifndef <token>
    def _rule_ifndef(self, args):
        self._flow_enter_if(not self.is_defined(args[0]))

    # Rule to handle: #else
    def _rule_else(self, args):
        self._flow_else_if(True)

    # Rule to handle: #elif <expression>
    def _rule_elif(self, args):
        self._flow_else_if(self._test_expression(args[0]))

    # Rule to handle: #endif
    def _rule_endif(self, args):
        self._flow_exit_if()

    # Rule to handle: #include <file> OR #include "file"
    def _rule_include(self, args):
        fname = args[0]
        if self.include_rule(fname):
            self.include(fname, self.ignore_missing_includes)

    # Rule to handle: #error <any>
    def _rule_error(self, args):
        raise Exception("#error {0}".format(args[0]))

    # Rule to handle: #undef <token>
    def _rule_undef(self, args):
        self.undefine(args[0])

    # Rule to handle: #pragma <any>
    def _rule_pragma(self, args):
        match = re.match(r"^python\s+\"([^\"]*)\"", args[0])
        if match:
            expr = match.groups()[0]
            eval(expr)
    
    #
    #     EXPRESSION EVALUATION
    #

    # Expands any macros in an expression
    def _expand_macros(self, expr):
        return expr
    
    # evaluates an expression down to its components. Non trivial.
    def _evaluate_expression(self, expr):

        # Should first prioritise by brackets and operators.
        match = re.match(r"(\w+)\s*\(([^)]+)\)", expr)
        if match:
            function, args = match.groups()
            return self._evaluate_function(function, [arg.strip() for arg in args.split(',')])

        return self._evaluate_token(expr)

    def _evaluate_function(self, function, args):
        if function == "defined":
            if len(args) != 1:
                return False
            return self.is_defined(args[0])
        # handle varidic macros here.
        return False

    def _evaluate_token(self, token):
        if token[0].isdigit():
            if token.startswith("0x"):
                return int(token[2:], 16)
            if token.startswith("0b"):
                return int(token[2:], 2)
            if token.startswith("0"):
                return int(token[1:], 8)
            else:
                return int(token, 10)

        if token in self.macros:
            return self._evaluate_expression(self.macros[token])

        return token

    # For testing expressions found in macros
    def _test_expression(self, expr):
        result = self._evaluate_expression(expr)
        if type(result) is str:
            return False
        return bool(result)

    #
    #     FLOW EVALUATION
    #

    # Enters a new #if block
    def _flow_enter_if(self, enabled):
        self._enable_stack.append(self._content_enabled)
        if self._content_enabled == IF_STATE_NOW:
            self._content_enabled = IF_STATE_NOW if enabled else IF_STATE_SEEK
        else:
            # Our current if block is disabled. Stop us from matching any until we exit.
            self._content_enabled = IF_STATE_SKIP

    # Exist the last #if block
    def _flow_exit_if(self):
        if not len(self._enable_stack):
            raise Exception("Unexpected #endif reached")
        self._content_enabled = self._enable_stack.pop(-1)

    # Passes through an #else block
    def _flow_else_if(self, enabled):

        if self._content_enabled == IF_STATE_NOW:
            # we may no longer match other if blocks.
            self._content_enabled = IF_STATE_SKIP
        elif self._content_enabled == IF_STATE_SEEK and enabled:
            # Only accept the new state if we have not yet matched an if block.
            self._content_enabled = IF_STATE_NOW

    def _flow_enabled(self):
        return self._content_enabled == IF_STATE_NOW




