import os.path
from preprocessor import Preprocessor

#LIB_ROOT = "../../Lib"
LIB_ROOT = "c:/dev/STM32X/Lib"


class UsbClass():
    def __init__(self):
        pass

def collect_stats(classname):
    header_path = '/'.join([LIB_ROOT, "usb", classname.lower(), "USB_{}.h".format(classname)])

    parser = Preprocessor()
    parser.digest(header_path)
    print(parser.macros)

if __name__ == "__main__":

    parser = Preprocessor()
    parser.ignore_missing_includes = True
    parser.add_include_path( os.path.join(LIB_ROOT,"usb") )
    parser.define("USB_CLASS_MSC")
    parser.include("USB_Class.h")
    print(parser.varidic_macros)
    #print(parser.source())
    #print(parser.evaluate("USB_INTERFACES"))

