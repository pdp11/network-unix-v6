|   This macro is used to combine a set of files into one file for
|convenience in moving, ftping, etc.  The files are concatenated, separated
|by a line containing the flag sequence >=> followed by the file name.
|On Unix, this may be a complete path name.  The file format is compatible
|with the 10x/tops20 combine and separate programs, if only single-level
|file names are used.
|
|   The macro is controlled by a file which contains file names, one
|per line.  The first file name will be the output file.  The remainder
|are the files to be inserted.
|
|   To run the macro, do ercmb.t$yhxm to get the macro into q-reg m.
|Then do erfile$mm$$  where file is the name of the file being processed.
|The macro will print out the name of each file as it goes.
|It uses q-regs a,b,c,d, and e during its processing, but saves and restores.
|any previous contents.

[a[b[c[d[e
yOutput to t j iev l-c 27i j 1xa ma 1k hxa hk  |Open output file
  <ga j .-z; 1xb k hxa hk gb    |Get next input, if none, exit
   File:  l-c 0t  ...  j
   ierl-c27i j 1xc mc         |Open input
   y j zue 62i 61i 62i gb       |Form separator with file name
   <+1"E zj12i %e '           |Add form feed if needed
    hp y qe+zue .-z; >          |Loop until no more input
   Complete, byte count:  qe=
  >
Done.
 ef ]e]d]c]b]a
