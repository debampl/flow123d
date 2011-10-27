# 
# Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
#
# Please make a following refer to Flow123d on your project site if you use the program for any purpose,
# especially for academic research:
# Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
#
# This program is free software; you can redistribute it and/or modify it under the terms
# of the GNU General Public License version 3 as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program; if not,
# write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
#
# $Id$
# $Revision$
# $LastChangedBy$
# $LastChangedDate$
#
# This makefile just provide main rules for: build, documentation and testing
# Build itself takes place in ./src.
#

include makefile.in
include makefile.include


all: bin/mpiexec revnumber bin/current_flow
	make -C third_party all
	make -j $(MAKE_JOBS) -C src all
	
bin/mpiexec: makefile.in
	# TODO:
	# some time PETSC don't set MPIEXEC well
	# then we should detect existence of PETSC_ARCH/bin/mpiexec
	# last chance is to use system wide mpiexec
	# or die
	 
	if which "${MPIEXEC}"; then \
	    echo '#!/bin/bash' > bin/mpiexec; \
	    echo '"${MPIEXEC}" "$$@"' >> bin/mpiexec; \
	elif [ -x "${PETSC_DIR}/${PETSC_ARCH}/bin/mpiexec" ]; then \
	    echo '#!/bin/bash' > bin/mpiexec; \
	    echo '"${PETSC_DIR}/${PETSC_ARCH}/bin/mpiexec" "$$@"' >> bin/mpiexec; \
	else \
	    echo "Can not guess mpiexec of PETSC configuration"; \
	fi        
	chmod u+x bin/mpiexec

bin/current_flow:
	if [ -z "${MACHINE}" ]; then \
		echo "Using default: current_flow"; \
		echo '#!/bin/bash' > bin/current_flow; \
		echo "\"`pwd`/bin/generic_flow.sh\"" >> bin/current_flow; \
	else \
		if [ -e "bin/stub/${MACHINE}_flow.sh" ]; then \
			echo '#!/bin/bash' > bin/current_flow; \
			echo '"`pwd`/bin/stub/${MACHINE}_flow.sh"' >> bin/current_flow; \
		else \
			echo "script for given MACHINE not found, using default"; \
			echo '#!/bin/bash' > bin/current_flow; \
			echo '"`pwd`/bin/stub/generic_flow.sh"' >> bin/current_flow; \
		fi \
	fi
	chmod u+x bin/current_flow
		#echo '"${PWD}/${BUILD_DIR}/bin/generic_flow.sh"' >> bin/current_flow; \
	
revnumber:
	if which "svnversion" ;\
	then echo "#define REVISION \"`svnversion`\"" >include/rev_num.h;\
	else echo "#define REVISION \"`bin/svnversion.sh`SH\"" >include/rev_num.h;\
	fi

# Remove all generated files
clean:
	make -C src clean
	make -C doc/doxy clean
	make -C tests clean
	rm -f bin/mpiexec
	rm -f bin/current_flow

# Make all tests	
testall:
	make -C tests testall

# Make only certain test (eg: make 01.tst will make first test)
%.tst :
	make -C tests $*.tst

# Create doxygen documentation
online-doc:
	make -C doc/doxy doc
