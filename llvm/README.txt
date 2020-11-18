The LLVM Compiler Infrastructure
================================

This directory and its subdirectories contain source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and runtime environments.

LLVM is open source software. You may freely distribute it under the terms of
the license agreement found in LICENSE.txt.

Please see the documentation provided in docs/ for further
assistance with LLVM, and in particular docs/GettingStarted.rst for getting
started with LLVM and docs/README.txt for an overview of LLVM's
documentation setup.

If you are writing a package for LLVM, see docs/Packaging.rst for our
suggestions.


Assignment tracking prototype
=============================
This prototype is not intended to be reviewed. It accompanies an RFC titled
"Assignment tracking: A better way of specifying variable locations in IR" with
the intention that the prototype can be run by anyone interested. There's a
little bit of info about the new intrinsics in
llvm/docs/SourceLevelDebugging.rst.

You can build clang from this source normally (llvm/docs/CMake.rst) or copy the
top patch to your existing llvm source and build from there. The prototype has
been squashed into one patch that applies cleanly onto
upstream/release/14.x. Using clang built with this patch you can enable
assignment tracking by adding -Xclang -debug-coffee-chat to the command line.

"Coffee chat" and "assignment tracking" are used interchangeably in this
prototype as "coffee chat" was the name of the project before I came up with a
suitable name.

It's not optimised in the slightest so compiling large programs is likely to
take a long time and memory consumption can be high. You can lower -mllvm
-coffee-max-blocks from its default of 1500 to drop debug intrinsics from
functions with a large number of blocks, which may help.

For reference, compiling the CTMark projects on my VM using 6 cores takes 1 min
45 seconds without assignment tracking and nearly 3 minutes with it
enabled. Most of this increase comes from compiling tramp3d-v4, which doubles
from 1 minute to 2.

If anyone is interested in looking at the code changes I'll happily outline the
interesting parts. The code changes come to roughly 4k loc with the rest just
being tests (including some not-yet-reduced ones). Feel free to get in touch
via Discourse (@OCHyams) by commenting on the RFC.
