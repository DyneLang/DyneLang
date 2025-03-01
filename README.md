# DyneLang

## The Dyne Language: Object System, Compiler, and Interpreter

Dyne is a prototype-based programming language inspired by NewtonScript. 
Originally developed by Walter Smith at Apple in 1994, NewtonScript powered 
software for the Newton PDA. NewtonScript is heavily influenced by the Self 
programming language.

Dyne is designed to maintain backward compatibility with NewtonScript while 
modernizing the language for contemporary computing. It extends the original 
language, introduces new features, and provides full 64-bit compatibility.

## DyneE5, DyneOS, and DynePDA

Dyne is part of a bigger ecosystem that includes the SYne operating system 
with a database style file system, a minimal implementation of a PDA that 
can install and run Dyne Packages, and DyneE5, a specialised IDE that includes
a GUI editor to write apps for the PDA.

## The Language

Dyne is a text based language that is compiled into objects that hold platform
independent byte code. Objects are stored in Packages that can be installed
on the PDA or run using the byte code interpreter.

At this time, DyneLang is a subset of NewtonScript. To get more information 
on NewtonScript, please visit https://newtonscript.org .

# dynec

"dynec" is a tool designed to read NewtonOS Package files, analyze their 
contents, and convert them into an editable text format (ARM32 assembler). 
Once modified, GNU `as` and GNU `objcopy` can reconstruct a valid Package file 
from the assembler file.

The primary goal is to enable the conversion of any Package file into one or 
more easily editable files. NOS parts should be written as NewtonScript source 
files, while embedded images and sounds should be exported in standard formats.

This approach allows for a deeper understanding of existing Packages, 
facilitates debugging and modifications, and enables the creation of entirely 
new Newton and later DyneLang applications from scratch. 

To verify `dynec`, unpacking and repacking a  Package should yield an identical
file, ensuring fidelity in the conversion process.

## Current Status

The current implementation can read Package files and interpret NOS parts
correctly. It generates an ARM32 assembly file, which can be assembled 
into an object file using the GNU assembler. The GNU objcopy tool can then 
extract the .data segment as binary, effectively reconstructing 
the original Package file.

All data written to the assembly file uses symbols instead of direct 
numerical indexing. This allows users to modify assembler files — such as 
inserting or removing data — without disrupting the structure of the Package, 
even if addresses and offsets change.

Furthermore, `dynec` can generate a Dyne Object Tree from NOS Parts in the 
Package and output it as DyneScript text.

## Next Steps

Decompile functions.

### Next

- ✓ binary compare ignoring alignment filler
- ✓ run through test files for verification
- ✓ write bytecode (it's good enough to write them into comments)
- ✓ verify reading bytecode
- ~ symbolic offsets in bytecode (not now)
- verify that modified files still load and run correctly

### Next I want to generate a Newton Object Tree from the binary data:

- ✓ implement a rough Newton Object System (nos)
- ✓ generate Newton Object tree from package
- write documentation for all new methods
- create package from Newton Object tree
- compare binary data from original package and Object Tree generated package

### Next I want to be able to decompile and compile:

- ✓ generate NewtonScript text from Object Tree, data only (no byte code, print 
  binary objects in hex)
- convert byte code back into source code
  - ✓ basic layout
  - implement `ToString(Ref)`
  - incremental implementation of 58 byte code commands
- generate Object Tree from UTF-8 NewtonScript (simple compiler)
- test by writing a Package from NewtonScript
- compile source code into byte code
- run byte code

### Side Note

"The 2.0 interpreter has numerous optimizations over the 1.0 
version that required a slightly different function format. A 2.0 argFrame 
contains only what's necessary to make closures work. The _parent, 
_nextArgFrame, and _implementor slots can contain NIL if the function doesn't
use them. Most important, a 2.0 argFrame will have a slot for a local variable 
only if the variable is referenced in another function and so must be present 
in a closure. Purely local variables are kept on the stack in 2.0.

NumArgs in a 2.0 function has two bitfields. The lower 14 bits gives the
number of arguments, and the upper 16 bits gives the number of locals to
allocate on the stack." - Walter Smith

### A little extra fun:

At this point, we can add a reader and writer for ROM data pretty easily. We 
may need some additions to NewtonScript to define Magic Pointers and find
global addresses in ROM. 

### Now we can incrementally improve the compiler and decompiler:

- interpret binary data and write it in a more human readable format:
  - find images and write them a png image files
  - find sounds and write them as aif or similar
  
### Add the interpreter:
  
- read external data into the Object Tree
  - add a function that reads a png image and generates a NewtonOS bitmap
  - to integrate this into NewtonScript, we need a working interpreter
  
### Improve the decompiler:

- write ByteCode into the NewtonScript text
- read ByteCode in the compiler
- incrementally improve the decompiler to write NewtonScript code instead 
  of byte code
- ensure that the generate NewtonScript generates the original ByteCode 
  when compiled
  
### Add NTK functionality:

NTK has a few more functions that we probably need to implement while doing the 
steps above. Now is the point where we can implement everything needed to
compile NTK projects. This would require readers for all NTK formats.

### Add a NewtonOS environment:

I know that this is last in the list, but I am sure that we will get a minimal
OS environment running as soon as the interpreter works. Combined with a
GUI library (hello FLTK), we can incrementally implement all widgets that our
test apps need.  

## Long-Term Goals

Generating assembly files serves as a verification step to ensure that all 
aspects of the Package format are correctly understood. Given the existence 
of thousands of Newton Packages in the wild, processing them with "newtfmt" 
should help identify all variations of the format and refine both the 
reader and writer components.

The next objective is to reverse-engineer all Newton Packages into their 
most original and human-readable format. This will require integrating both 
a decompiler and compiler, along with a functional NewtonScript interpreter.

Furthermore, "newtfmt" should also be capable of extracting all 
NewtonScript code from ROM images and translating it into ARM32 assembly. 
This capability would significantly aid efforts to fix bugs within the ROM, 
particularly in light of the looming Year 2040 (Y2K40) time-related issues.

Ultimately, "newtfmt" could evolve into a NewtonOS-compatible environment, 
making it possible to run NewtonScript applications natively 
without requiring an emulator.


## Findings

A list of findings in existing Newton Packages as found in the UNNA archive.

### Duplicate Symbols

Some NOS Parts defined the same Symbol multiple times. This is just some lack
of optimization by NTK or another tool, possibly caused by the debug flag 
being enabled and it is not harmful.

### Non-ASCII Symbols

I found exactly one package that uses a `(tm)` Mac Roman character in a symbol:
"/Users/matt/Azureus/unna/games/SuperNewtris2.0/SNewtris.pkg"

### Dirty Flag

Some Objects in patches have the Dirty flag set. This seems to be specific 
for system patches.

### Schlumberger Watson

Schlumberger Packages have an additional Part of type Raw containing the 
signature "xxxxSLB0Schlumberger Industries\0" and set the 
kWatsonSignaturePresentFlag in the Package flags. Watson will not load a Package
that does not have these modifications.  

### Invalid References

I found a few NOS Parts that contain Refs with invalid offsets. It seem that 
all (or some) Refs have an offset at 0x60xxxxxx, suggesting they were extracted
from a live installation. These references are generated when a package is read 
back from Newton Memory and the relocation software (`rex`, for example), 
misses the Ref. This is fixable if the original offset can be found and applied.

Here are a few packages with that issue:

- "/Users/matt/Azureus/unna/applications/calculator/GoFigure.pkg"
- "/Users/matt/Azureus/unna/applications/Mapper/SF Mapv2.0-stk.pkg"
- "/Users/matt/Azureus/unna/applications/ShiftWork2/ShiftWork2.pkg"
- etc.

### Unreferenced Objects

Some packages have objects that don't seem to be referenced by any Ref in
the package. These objects can never be reached and will not be found and
deleted by the garbage collection. They seem to just be wasting space and
can probably be removed from the package file.  

## Not on the List

These issues are currently not in the scope of this app. Still the binary
reader and writer recognizes those Parts and transfers them verbatim. 

### Protocol Part:
- "/Users/matt/Azureus/unna/applications/MADNewton1.0.1/MP3Codec.pkg"
- etc.

### Raw Parts:
- "/Users/matt/Azureus/unna/applications/Fortunes/Science.pkg"
- etc.

### Assembler File:
 
Assembler output is almost entirely symbolic, notable with the exception of the 
byte code output. Also, relocation data can not be symbolic because it is
encoded in a block scheme. It is rarely used (but if it *is* used, usually to
implement copy protection schemes).
 
We could add an ARM32 disassembler if we want to dive deeper into that. 


## History

- Jan '25: initial commit, package to binary, binary to asm, asm to package
- Feb '25: adding Newton Object System, nos, using modern C++ to use constexpr 
           for generating the default object system at compile time.

 — Matthias

