# Snow Programming Language

## How to build

### Getting the source

    $ git clone <repository url>/snow.git snow

### Getting LLVM 2.9

To build Snow, you need the LLVM source code. Snow is currently built against LLVM 2.9.

    $ cd snow
    $ svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_29/final llvm

You will also need the Clang compiler, as Snow uses it to compile its runtime.

    $ cd llvm/tools
    $ svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_29/final clang

Now it's time to configure and build LLVM and Clang:

    $ cd ..
    $ ./configure --enable-assertions --enable-optimized --enable-targets=x86_64
    $ make

Substitute x86_64 for your appropiate architecture. 

### Building Snow

    $ cd snow
    $ make

If you followed the steps above, the Snow build process should automatically be able to find the necessary LLVM files. 
