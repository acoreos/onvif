# Makefile to build all programs in all subdirectories
#
# DIRS is a list of all subdirectories containing makefiles
# (The library directory is first so that the library gets built first)
#

DIRS = h264
#DIRS = discovery \
       deviceinfo \
       systemtime \
       capabilities \
       avstream \
       snapshot \
       SetResolution \

# Dummy targets for building and clobbering everything in all subdirectories

all: 	
	@ for dir in ${DIRS}; do (cd $${dir}; ${MAKE}) ; done

clean: 
	@ for dir in ${DIRS}; do (cd $${dir}; ${MAKE} clean) ; done
