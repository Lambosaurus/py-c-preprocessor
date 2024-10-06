import os.path
import sys

# Hack to include module in base directory
sys.path.insert(0, os.path.abspath('./'))
from preprocessor import Preprocessor

SRC_PATH = "tests/test_src"

def test_assert(expr, expected):
    if expr != expected:
        raise AssertionError("Expected {}, got {}".format(expected, expr))



# Tests that a macro can be evaluated
def test_macro_evaluation():
    p = Preprocessor()

    p.define("MACRO_CONST", "0x1")
    p.define("MACRO_A", "(a + b)", ["a","b"])
    p.define("MACRO_B", "(a + MACRO_CONST)", ["a"])
    p.define("MACRO_C", "(MACRO_A(a, 1) + MACRO_B(b))", ["a", "b"])
    p.define("MACRO_D", "(v & (512 - 1))", "v")
    p.define("MACRO_E", "23", [])

    # test basic macro evaluation works
    test_assert(p.evaluate("(3 + 4) / 2"), 3)
    test_assert(p.evaluate("MACRO_CONST + 1"), 2)
    test_assert(p.evaluate("MACRO_A(1, 2)"), 3)
    test_assert(p.evaluate("MACRO_B(10)"), 11)
    test_assert(p.evaluate("MACRO_C(1, 2)"), 5)
    test_assert(p.evaluate("MACRO_D(512 + MACRO_CONST)"), 1)
    test_assert(p.evaluate("MACRO_E()"), 23)

    # Function-like macros should not be expanded if no arguments are provided.
    test_assert(p.expand("MACRO_E"), "MACRO_E")

    # a few more tests to check evaulation
    test_assert(p.evaluate("3 - 4"), -1)
    test_assert(p.evaluate("3 == 5"), False)
    test_assert(p.evaluate("3 != 5"), True)
    test_assert(p.evaluate("!(1)"), False)

    test_assert(p.evaluate("defined(MACRO_Z)"), False)
    test_assert(p.evaluate("defined(MACRO_A)"), True)


# Tests that a recursive macro does not block execution
def test_recursive_macro():
    p = Preprocessor()

    p.define("MACRO_A", "MACRO_B")
    p.define("MACRO_B", "MACRO_A")

    try:
        p.evaluate("MACRO_A")
        test_assert("The expression above should fail.", None)
    except:
        pass


# Tests for conditional directives
def test_conditional_directives():
    src = """
    #if defined(CASE_A)
    #define MACRO_A 1
    #elif (CASE_B == 1)
    #define MACRO_A 2
    #else
    #define MACRO_A 3
    #endif
    """

    p = Preprocessor()
    p.define("CASE_A")
    p.include("source.c",src)
    test_assert(p.evaluate("MACRO_A"), 1)

    p = Preprocessor()
    p.define("CASE_B", "1")
    p.include("source.c",src)
    test_assert(p.evaluate("MACRO_A"), 2)

    p = Preprocessor()
    p.undefine("CASE_B")
    p.include("source.c",src)
    test_assert(p.evaluate("MACRO_A"), 3)

# Tests for checking that #directives with spaces still work
def test_spaced_directives():
    p = Preprocessor()
    src = """
    #define SYMBOL_A 1
    # define SYMBOL_B 2
    #        define SYMBOL_C 3
    #define         SYMBOL_D 4
    #define SYMBOL_E         5
    """
    p.include("source.c", src)
    # Just check that all symbols got defined, and can be resovled.
    test_assert(p.expand("SYMBOL_A,SYMBOL_B,SYMBOL_C,SYMBOL_D,SYMBOL_E"), "1,2,3,4,5")

# Tests for including a file
def test_include():
    p = Preprocessor()
    p.add_include_path(SRC_PATH)
    p.include("test.h")
    
    test_assert(p.evaluate("MACRO_A(1, 2)"), 3)
    test_assert(p.evaluate("MACRO_B(1)"),   2)
    test_assert(p.evaluate("MACRO_C(1, 2)"), 5)
    test_assert(p.evaluate("MACRO_D(513)"), 1)

def test_whitespace_strip():
    p = Preprocessor()
    src = """
    #define MACRO_SPACED_PARAMS(a,   b, c,d) a b c d
    #define MACRO_SPACED_ARGS(a, b, c, d) a b c d
    """
    p.include("source.c", src)

    test_assert(p.expand("MACRO_SPACED_PARAMS(1, 2, 3, 4)"), "1 2 3 4")
    test_assert(p.expand("MACRO_SPACED_ARGS(1,   2,3, 4)"), "1 2 3 4")
    
# Tests for variadic parameters in macros
def test_va_args():
    p = Preprocessor()
    p.define("MACRO_VA_ARG_IDENTITY", "__VA_ARGS__", ["..."])
    p.define("MACRO_NAMED_VA_ARG_IDENTITY", "x", ["x..."])
    p.define("MACRO_VA_ARG_COHERENCE", "a@x", ["a", "x..."])
    p.define("MACRO_VA_ARG_INVALID", "a@x", ["a...", "x"])
    p.define("MACRO_VA_ARG_INVALID2", "a@x", ["a...", "x..."])
    p.define("MACRO_INVALID", "a@x", ["a", "x...."])
    
    test_assert(p.expand('MACRO_VA_ARG_IDENTITY(1, 2 3, "abc")'), '1, 2 3, "abc"')
    test_assert(p.expand('MACRO_NAMED_VA_ARG_IDENTITY(1, 2 3, "abc")'), '1, 2 3, "abc"')
    # Note that spaces are not .strip()-ped in variadic macros
    test_assert(p.expand("MACRO_VA_ARG_COHERENCE(contact test,domain.tld, or call +0123456789 for further assistance)"), 
                "contact test@domain.tld, or call +0123456789 for further assistance")
    
    try:
        p.evaluate("MACRO_VA_ARG_INVALID(a b c, d, e)")
        test_assert("The expression above should fail.", None)
    except:
        pass

    try:
        p.evaluate("MACRO_VA_ARG_INVALID2(a b c, d, e)")
        test_assert("The expression above should fail.", None)
    except:
        pass

    try:
        p.evaluate("MACRO_INVALID(a b c, d, e)")
        test_assert("The expression above should fail.", None)
    except:
        pass

# tests that macros with embedded in strings are correctly handled
def test_string_embedded_macros():
    p = Preprocessor()
    p.define("MACRO_CONST", "0x1")
    p.define("MACRO_A", "(a + b)", ["a","b"])
    p.define("MACRO_B", "(a + 1)", ["a"])

    # check for macro expansion in strings
    # Note, this is incorrect logic for C string gluing, but it is a good test
    test_assert(p.evaluate('MACRO_A("TEXT ","MACRO_CONST")'), "TEXT MACRO_CONST")

    # check for macro expansion in strings
    test_assert(p.evaluate('"MACRO_A(1,MACRO_B(2))"'), "MACRO_A(1,MACRO_B(2))")

    # check for parenthesis and commas in strings
    test_assert(p.evaluate('MACRO_A("TEXT, ", ")")'), "TEXT, )")

    # check for escaped symbols in strings
    test_assert(p.evaluate('MACRO_A("\'\\\\ \\" ","TEXT")'), "'\\ \" TEXT")


# tests that macros with with nested arguments are correctly handled
def test_nested_macros():
    p = Preprocessor()
    p.define("MACRO_CONST", "0x1")
    p.define("MACRO_A", "(a + b)", ["a","b"])
    p.define("MACRO_B", "(a + 1)", ["a"])
    p.define("MACRO_C", "MACRO_B")

    # check for nested macros
    test_assert(p.evaluate("MACRO_A(1,MACRO_B(2))"), 4)

    # check alternate spacing
    test_assert(p.evaluate("MACRO_A ( 1, MACRO_CONST )"), 2)

    # try other orientation
    test_assert(p.evaluate("MACRO_A(MACRO_B( 2 ), 1)"), 4)

    # check that nested macros with commas work
    test_assert(p.evaluate("MACRO_A(1, MACRO_A(3,4))"), 8)

    # check a heavily nested macro
    test_assert(p.evaluate("MACRO_A(1, MACRO_B(MACRO_A(3,MACRO_B(1))))"), 7)

    # A non arg macro that expands into an argument macro
    test_assert(p.evaluate("MACRO_C(1)"), 2)


# Test that source is correctly expanded
def test_source_expansion():
    p = Preprocessor()

    # Include a piece of source with some macros to be expanded.
    # Note the multiline macro expansion.
    
    p.define("MACRO_CONST", "3")
    p.include("main.c", """

    #define MACRO_A(a,b) (a + b)
    #define MACRO_B(a,b) MACRO_A(a, MACRO_A(1, b))

    int void main(void)
    {
        int a = MACRO_A(1,2);
        return MACRO_B(
            a,
            MACRO_CONST
        );
    }

    """)

    expected = """
    int void main(void)
    {
        int a = (1 + 2);
        return (a + (1 + 3));
    }
    """

    # Remove whitespace - too much of a pain to test.
    def trim_whitespace(s):
        return " ".join(s.split())
    
    # Check that the source is expanded correctly
    source = p.source()
    test_assert(trim_whitespace(source), trim_whitespace(expected))


# Real world test cases using the USB MSC example
def test_usb_class_msc():
    p = Preprocessor()
    p.ignore_missing_includes = True
    p.add_include_path(SRC_PATH)

    p.define("USB_CLASS_MSC")
    p.include("usb/USB_Class.h")
    
    test_assert(p.expand("USB_INTERFACES"), "1")
    test_assert(p.expand("USB_ENDPOINTS"), "2")
    test_assert(p.expand("USB_CLASS_DEVICE_DESCRIPTOR"), "cUSB_MSC_ConfigDescriptor")
    test_assert(p.expand("USB_CLASS_INIT(0)"), "USB_MSC_Init(0)")

# Real world test cases using the USB CDC example
def test_usb_class_cdc():
    p = Preprocessor()
    p.ignore_missing_includes = True
    p.add_include_path(SRC_PATH)

    p.define("USB_CLASS_CDC")
    p.include("usb/USB_Class.h")
    
    test_assert(p.expand("USB_INTERFACES"), "2")
    test_assert(p.expand("USB_ENDPOINTS"), "3")
    test_assert(p.expand("USB_CLASS_DEVICE_DESCRIPTOR"), "cUSB_CDC_ConfigDescriptor")
    test_assert(p.expand("USB_CLASS_INIT(0)"), "USB_CDC_Init(0)")

# Include a source file with a lot of source
def test_include_source():
    p = Preprocessor()
    p.ignore_missing_includes = True
    p.add_include_path(SRC_PATH)

    p.include("usb/cdc/USB_CDC.c")
    p.source()

# Run all the tests
def run_tests():
    test_macro_evaluation()
    test_recursive_macro()
    test_conditional_directives()
    test_spaced_directives()
    test_include()
    test_whitespace_strip()
    test_va_args()
    test_string_embedded_macros()
    test_nested_macros()
    test_source_expansion()
    test_usb_class_msc()
    test_usb_class_cdc()
    test_include_source()

if __name__ == "__main__":
    run_tests()
