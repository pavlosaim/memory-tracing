# memory profiling and tracing tool

## original
This repository contains gleipnir tool, created originally from Tomislav (Tommy) Janjusic.
The original repository can be found on http://csrl.cse.unt.edu/content/gleipnir

## memory tracing 
In this repo, based on gleipnir tool, some features are removed and some have changed. This repository contains code mainly written for mapvisual (https://github.com/pavlosaim/mapvisual) repository.

## installation
Clone git repo to a folder named "gleipnir".
Before making Valgrind with Gleipnir, on gleipnir folder.

Apply debug patch and makefile patches.

$./glconf.sh -d

This will patch debuginfo.c and pub_tool_debuginfo.h for gleipnir's use, and update Valgrind's makefiles and conf files.

Malloc replace

replace - will NOT patch Valgrind, but configure Gleipnir to use gl_malloc_replacement.c (i.e. Allocation will call VG_(cli_malloc)()

$ ./glconf -m replace 

Applying the patch will not break other tools. (fingers crossed)

IMPORTANT: If you want to switch between the configurations (wrap vs replace), you have to distclean and start from top.

Back on valgrind main folder.
Edit Makefile.am and add "gleipnir" on TOOLS.

Proceed with valgrind installation process.

$ autogen.sh

$ ./configure --prefix = current_valgrind_dir

$ sudo make; sudo make install;

Gleipnir should now be installed with valgrind!
