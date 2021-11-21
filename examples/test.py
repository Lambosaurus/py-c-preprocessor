import os.path
import sys

# Hack to include module in base directory
sys.path.insert(0, os.path.abspath('./'))
from preprocessor import Preprocessor

SRC_PATH = "examples/test_src"

def test_assert(expr, expected):
    if expr != expected:
        raise AssertionError("Expected {}, got {}".format(expected, expr))

# Tests macros included from USB_CLASS_MSC
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

# Tests macros included from USB_CLASS_CDC 
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

# Tests that multi stage expressions can be evaluated
def test_expression_evaluation():
    p = Preprocessor()
    p.ignore_missing_includes = True
    p.add_include_path(SRC_PATH)

    p.define("USB_CLASS_CDC")
    p.include("usb/cdc/USB_CDC.c")

    test_assert(p.evaluate("CDC_BFR_WRAP(512)"), 0)





if __name__ == "__main__":

    test_usb_class_cdc()
    test_usb_class_msc()
    test_expression_evaluation()
