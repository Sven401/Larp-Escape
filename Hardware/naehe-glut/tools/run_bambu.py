"""Run the official Windows Bambu Studio CLI entry point, with console output.

The distributed GUI launcher exits without forwarding output in this environment.
The exported signature is documented in BambuStudio.cpp at tag v02.08.02.61:
int __stdcall bambustu_main(int argc, wchar_t **argv).
This wrapper changes no slicer behavior and never sends a job to a printer.
"""
import ctypes
import os
from pathlib import Path
import sys


def main():
    if len(sys.argv) < 3:
        raise SystemExit("Usage: run_bambu.py <BambuStudio directory> <CLI args...>")
    directory = Path(sys.argv[1]).resolve()
    arguments = [str(directory / "bambu-studio.exe"), *sys.argv[2:]]
    # Keep the search-directory handle alive through the native invocation.
    with os.add_dll_directory(str(directory)):
        library = ctypes.WinDLL(str(directory / "BambuStudio.dll"))
        entry = library.bambustu_main
        entry.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_wchar_p)]
        entry.restype = ctypes.c_int
        argv = (ctypes.c_wchar_p * (len(arguments) + 1))(*arguments, None)
        return entry(len(arguments), argv)


if __name__ == "__main__":
    raise SystemExit(main())
