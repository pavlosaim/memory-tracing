#!/bin/bash
# Gleipnir basic configure script.
# Tomislav Janjusic
# tjanjusic [at] unt [dot] edu
# janjusict [at] ornl [dot] gov

if [ $# -eq 0 ]
	then
		echo "No arguments:"
		echo "    -d (OPTION)            Apply debug patch."
		echo "    -p (OPTION)            Apply malloc patch."
		echo "    -m (ARG:wrap/replace)  Set malloc type. [replace]"
		exit 0
fi

while getopts :pdcm: args; do
	case $args in
	  d)
			if [ ! -f debugpatch.log ]
				then
					echo -e "Patching debug...";
					date > debugpatch.log;
					patch -d ../ -p0 < ./patches/debuginfo.patch >> debugpatch.log;
					patch -d ../ -p0 < ./patches/valgrind_conf_make.patch >> debugpatch.log;
					echo -e "Patching complete (debugpatch.log).";
				else
				 echo "Patch debug previously applied (debugpatch.log).";
			fi
			;;
		p)
			if [ ! -f mallocpatch.log ]
				then
					echo -e "Patching malloc...";
					date > mallocpatch.log;
					patch -d ../ -p0 < ./patches/wrapmalloc.patch >> mallocpatch.log;
					echo -e "Patching complete (mallocpatch.log).";
				else	
					echo "Patch malloc previously applied (mallocpatch.log).";
			fi
			;;
		m)
			read glconf_line < gl_configure.h;
			if [ $OPTARG == "wrap" ]
				then 
					if [ "$glconf_line" != "#define GL_MALLOC_WRAPPER 1" ]
					then
						echo "Setting gl_configure.h for malloc() wrapper.";
						echo "#define GL_MALLOC_WRAPPER 1" > gl_configure.h;
						patch -f -R Makefile.am < ./patches/Makefile.am.lib.patch;
						if [ ! -f mallocpatch.log ]
							then
								echo -e "Don't forget to apply the malloc patch $./glconfig -p !\n";
						fi
					else
						echo -e "Gleipnir is already configured for malloc wrapping\n";
					fi
			elif [ $OPTARG == "replace" ];
				then
					if [ "$glconf_line" != "#define GL_MALLOC_REPLACEMENT 1" ]
					then
						echo "Setting gl_configure.h for malloc() replacement.";
						echo "#define GL_MALLOC_REPLACEMENT 1" > gl_configure.h;
						patch -f Makefile.am < ./patches/Makefile.am.lib.patch
					else
						echo -e "Gleipnir is already configured for malloc replacement\n";
					fi
			else
					echo "-m (unknown argument)";
			fi
			;;
		?)
			echo -e "Usage: [-dp] [-m wrap/replace]\n"
			echo -e "Usage: [-d]\n"
			echo "    -d (OPTION)            Apply debug patch."
      echo "    -t (DEFUNCT)           Apply hugepage table patch."
			echo "    -p (OPTION)            Apply malloc patch."
			echo "    -m (ARG:wrap/replace)  Set malloc type. [replace]"
			exit 2
			;;
		esac
done

if [ ! -f gl_configure.h ]
	then
		echo -e "gl_confgure.h not found\n";
fi
