import re
import os.path
import io

IF_STATE_NOW  = 0
IF_STATE_SEEK = 1
IF_STATE_SKIP = 2

TOKEN_SEARCH_REGEX = re.compile(r"(\b(?<!(?:>|\.))\w+(?!->|\.)\b)") # to avoid matching struct-like fields
                                                                    # or variables dereference with the same name as defined macro
PAREN_SEARCH_REGEX = re.compile(r"\s*\(")

class Directive():
    def __init__(self, pattern, action, conditional = False):
        self.pattern = re.compile(pattern)
        self.action = action
        self.conditional = conditional

    def invoke(self, line, parse_enabled):
        # conditional directives must be checked even on disabled parse
        if parse_enabled or self.conditional:
            match = self.pattern.match(line)
            if match:
                self.action(match.groups())
                return True
        return False

class Macro():
    def __init__(self, token, expr = None, args = None):
        self.token = token
        self.expr = expr if expr else ""
        self.args = args
    
    def __repr__(self):
        if self.args:
            return "{}({}): {}".format(self.token, self.args, self.expr)
        return "{}: {}".format(self.token, self.expr)

    # Expands the macros to its full expression
    def expand(self, args = None):
        if args:
            return self._substitute_args(self.expr, args)
        return self.expr

    # Substitutes any defined arguments in the expression
    # This must be done in a single pass, so that nested tokens are left in place
    def _substitute_args(self, expr, args):
        # Create a map between the argument name and the value
        tokens = { self.args[i]: args[i] for i in range(len(self.args)) if not self.args[i] == '...'}
        if '...' in self.args:
            if self.args.count('...') > 1:
                raise ValueError('There can be only one variadic parameter.')
            
            if not '...' == self.args[-1]:
                raise ValueError('Variadic parameter should be the last in the list.')
            
            tokens.update({'__VA_ARGS__': ', '.join(args)})
        
        def _substitute_token(match):
            token = match.groups()[0]
            if token in tokens:
                return tokens[token]
            else:
                return token
        return TOKEN_SEARCH_REGEX.sub(_substitute_token, expr)


class Preprocessor():
    def __init__(self):
        
        self._directives = [
            # Conditional tokens
            Directive(r"#\s*if\s+(.*)", self._directive_if, True),
            Directive(r"#\s*ifdef\s+(\w+)", self._directive_ifdef, True),
            Directive(r"#\s*ifndef\s+(\w+)", self._directive_ifndef, True),
            Directive(r"#\s*elif\s+(.*)", self._directive_elif, True),
            Directive(r"#\s*endif", self._directive_endif, True),
            Directive(r"#\s*else", self._directive_else, True),

            # Standalone tokens
            Directive(r"#\s*pragma\s+(.*)", self._directive_pragma),
            Directive(r"#\s*error\s+(.*)", self._directive_error),
            Directive(r"#\s*include\s*\"([^\"]*)\"", self._directive_include),
            Directive(r"#\s*include\s*<([^>]*)>", self._directive_include),
            Directive(r"#\s*undef\s+(\w+)", self._directive_undef),
            
            # Define statements. Order is important.
            Directive(r"#\s*define\s+(\w+)\(([^\)]*)\)\s*(.*)?", self._directive_define_varidic),
            Directive(r"#\s*define\s+(\w+)\s*(.*)?", self._directive_define),
        ]

        self._content_enabled = IF_STATE_NOW
        self._enable_stack = []
        self._local_path = ""
        self._source_prior = None

        # special macro required to make the define statement work
        self._defined_macro = Macro("defined", "?", ["token"])
        self._defined_macro.expand = lambda args: "1" if self.is_defined(args[0]) else "0"

        self.macros = {}
        self.include_rule = lambda name: True
        self.include_paths = []
        self.ignore_missing_includes = False

        self.source_lines = []
        self.max_macro_expansion_depth = 4096

        self._ignored_definitions = []

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
        return "".join(self.source_lines)
    
    def ignore_macro_definitions(self, *defs):
        self._ignored_definitions.extend(defs)

    # Defines a symbol
    def define(self, token, expr = None, args = None):
        self.macros[token] = Macro(token, str(expr), args)
    
    # Undefines a symbol
    def undefine(self, token):
        if token in self.macros:
            del self.macros[token]

    # returns true if a preprocessor symbol is defined
    def is_defined(self, token):
        return token in self.macros

    # Consumes a file and preprocesses it.
    # file may be a string literal, or a file-like object, or None
    # If the file is not supplied, the path is used to find the file
    def include(self, path, file = None, may_ignore = False):
        if file is None:
            # Use the path for find the correct file
            path = self._resolve_path(path)
            if not os.path.exists(path):
                if may_ignore:
                    return
                else:
                    raise Exception("file \"{}\" cannot be found".format(path))
            file = open(path, "r")
        
        elif type(file) is str:
            # Treat the file as a literal body
            file = io.StringIO(file)

        # If the file is not a string, treat it as a file-like object
        self._include_file(file, path)
        file.close()

    #
    #     FILE PARSING
    #

    # Includes and processes the source in a file
    def _include_file(self, file, path):

        # Update the new local path to be relative to the current path.
        prior_local = self._set_local_path(path)
        stack_depth = len(self._enable_stack)

        prior_line = None
        in_comment = False

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
        if self._source_prior:
            self._source_prior = None
            raise Exception("unterminated macro expression")

        self._restore_local_path(prior_local)

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

    # Checks for preprocessor directives and invokes them.
    # Returns true if the line was consumed.
    def _preprocess_directives(self, line, enabled):
        line = line.strip()
        if line.startswith("#"):
            for directive in self._directives:
                if directive.invoke(line, enabled):
                    return True
        return False

    # Runs a line through the preprocessor
    def _preprocess_line(self, line):
        # check for directives
        enabled = self._flow_enabled()
        if not self._preprocess_directives(line, enabled):
            # if not a directive, then the line is source
            if enabled:
                if self._source_prior:
                    # glue the prior line to the new line
                    line = self._source_prior + line
                    self._source_prior = None
                line, self._source_prior = self._expand_macros(line)
                if line:
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
        self._local_path = os.path.normpath(os.path.dirname(path))
        return prior

    # Restores the local path to the previous value
    def _restore_local_path(self, prior):
        self._local_path = prior

    def _add_include_path(self, path):
        self.include_paths.append(os.path.normpath(path))

    #
    #      PREPROCESSOR DIRECTIVES
    #

    # Rule to handle: #define <token> [<expression>]
    def _directive_define(self, args):
        if not args[0] in self._ignored_definitions:
            self.define(args[0], args[1])

    # Rule to handle: #define <token>(<any>) [<expression>]
    def _directive_define_varidic(self, args):
        varargs = [ a.strip() for a in args[1].split(",") ]
        self.define(args[0], args[2], varargs)

    # Rule to handle: #if <expression>
    def _directive_if(self, args):
        self._flow_enter_if(self._test_expression(args[0]))

    # Rule to handle: #ifdef <token>
    def _directive_ifdef(self, args):
        self._flow_enter_if(self.is_defined(args[0]))

    # Rule to handle: #ifndef <token>
    def _directive_ifndef(self, args):
        self._flow_enter_if(not self.is_defined(args[0]))

    # Rule to handle: #else
    def _directive_else(self, args):
        self._flow_else_if(True)

    # Rule to handle: #elif <expression>
    def _directive_elif(self, args):
        self._flow_else_if(self._test_expression(args[0]))

    # Rule to handle: #endif
    def _directive_endif(self, args):
        self._flow_exit_if()

    # Rule to handle: #include <file> OR #include "file"
    def _directive_include(self, args):
        fname = args[0]
        if self.include_rule(fname):
            self.include(fname, may_ignore=self.ignore_missing_includes)

    # Rule to handle: #error <any>
    def _directive_error(self, args):
        raise Exception("#error {0}".format(args[0]))

    # Rule to handle: #undef <token>
    def _directive_undef(self, args):
        self.undefine(args[0])

    # Rule to handle: #pragma <any>
    def _directive_pragma(self, args):

        # custom pragma to execute python within the source. Useful for debugging.
        # #pragma python "print(p.macros)"
        # The current preprocessor instance is passed as the only local: p
        match = re.match(r"python\s+\"([^\"]*)\"", args[0])
        if match:
            expr = match.groups()[0]
            eval(expr, None, { "p": self })
    

    #
    #     MACRO EXPANSION
    #

    # Scans for the end of a string
    def _find_string_end(self, line, pos, endchar):
        while pos < len(line):
            if line[pos] == endchar:
                return pos + 1
            elif line[pos] == "\\":
                pos += 2
            else:
                pos += 1
        raise Exception("Unterminated string")

    # Looks for a closed pair of parentheses in the line.
    # If found, returns the index of the first character after the pair.
    # If not found, returns -1.
    def _find_parentheses_end(self, line, start):
        # find the matching parenthesis
        depth = 1
        i = start
        while i < len(line):
            if line[i] == '(':
                depth += 1
            elif line[i] == ')':
                depth -= 1
                if depth == 0:
                    return i + 1
            elif line[i] in "'\"":
                i = self._find_string_end(line, i+1, line[i])
                continue
            i += 1
        return None

    # Finds the arguments (<any>), taking care to skip embedded strings
    def _find_arguments(self, line, start):
        match = PAREN_SEARCH_REGEX.match(line, start)
        if match:
            start = match.end()
            end = self._find_parentheses_end(line, start)
            return start-1, end
        return None, None

    # Finds the next valid token to consider for macro replacement
    def _find_token(self, line, start):
        while True:
            # find a candidate token
            match = TOKEN_SEARCH_REGEX.search(line, start)
            if not match:
                break
            i = start
            start = match.start()
            while i < start:
                # if we hit a string, skip over it
                if line[i] in "'\"":
                    i = self._find_string_end(line, i+1, line[i])
                else:
                    i += 1
            
            if i > start:
                # Did we skip our token?
                # If so, it must have been in a stirng.
                start = i
            else:
                return match.span()
        return None, None

    # splits an argument string into a list of arguments
    # care should be taken not to split inside a string or parenthesis
    def _split_args(self, args):
        arglist = []
        i = 0
        arg_start = 0
        while i < len(args):
            if args[i] in "'\"":
                i = self._find_string_end(args, i+1, args[i])
            elif args[i] == '(':
                i = self._find_parentheses_end(args, i+1)
            elif args[i] == ',':
                arglist.append(args[arg_start:i].strip())
                arg_start = i + 1
                i += 1
            else:
                i += 1
        arglist.append(args[arg_start:].strip())
        return arglist


    # Expands all macros in the given expression
    def expand(self, expr):
        expr, remainder = self._expand_macros(expr)
        if remainder:
            raise Exception("Unterminated macro in expression")
        return expr
    
    # Expands all macros in the given expression
    # May return a remainder string if the expression is not fully expanded
    def _expand_macros(self, expr):
        expansion_depth = 0
        # expand macros
        start = 0
        while True:

            # find a token for consideration
            start, end = self._find_token(expr, start)
            if start == None:
                break
            token = expr[start:end]

            if token in self.macros:

                # check we arent caught in a loop
                if expansion_depth > self.max_macro_expansion_depth:
                    raise Exception(f"Max macro expansion depth exceeded (in expression \"{expr.strip().rstrip()}\")")

                # expand the macro
                macro = self.macros[token]
                if macro.args != None:

                    # find the arguments
                    arg_start, arg_end = self._find_arguments(expr, end)
                    if arg_start == None:
                        raise Exception("Macro \"{0}\" expects arguments (in expression \"{1}\")".format(token, expr.strip().rstrip()))
                    elif arg_end == None:
                        # We have an unterminated argument list.
                        # this line will have to be glued to the next line.
                        return None, expr

                    # separate the arguments
                    args = self._split_args(expr[arg_start+1:arg_end-1])

                    # support variadics
                    if len(args) != len(macro.args) and not '...' in macro.args:
                        raise Exception("Macro \"{0}\" requires {1} arguments (in expression \"{2}\")".format(token, len(macro.args), expr.strip().rstrip()))
                    
                    # replace the macro with the expanded expression
                    macro_expr = macro.expand(args)
                    end = arg_end
                else:
                    # expand the macro without arguments
                    macro_expr = macro.expand()

                # we have our new string
                expr = expr[:start] + macro_expr + expr[end:]

                # do not increase the start point - we should recheck this for new tokens to be expanded.
                expansion_depth += 1
            else:
                # proceed over the token
                start = end

        return expr, None

    #
    #     EXPRESSION EVALUATION
    #

    # Evaluates an expression
    def evaluate(self, expr):

        self.macros["defined"] = self._defined_macro
        expr = self.expand(expr)
        del self.macros["defined"]

        # convert to python expression (this may be very dangerous)
        expr = expr.replace("&&", " and ")
        expr = expr.replace("||", " or ")
        expr = expr.replace("/", "//")
        expr = re.sub(r"!([^?==])", r" not \1", expr)
        
        result = eval(expr)
        return result

    # Tests an expression for truth.
    def _test_expression(self, expr):
        try:
            result = self.evaluate(expr)
            if type(result) is str:
                return False
            return bool(result)
        except:
            return False

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

    # returns true if the current #if block is enabled
    def _flow_enabled(self):
        return self._content_enabled == IF_STATE_NOW
