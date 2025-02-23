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

...

```
dyn/
  types.h
  objects.h
  refs.h
  io/
    package.h
    stream.h
  lang/
    compiler.h
    interpreter.h
    decompiler.h
```

```
dyn/os
dyn/ui/widgets
dyn/io/soups
```
```
dyn/pda
dyn/pda/launcher
dyn/pda/notepad
```