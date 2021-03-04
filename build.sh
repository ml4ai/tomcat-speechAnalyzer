#/bin/bash

if [[ ! -d "./lib" ]] 
then
	mkdir lib
	cd lib

	#openSMILE
	git clone https://github.com/audeering/opensmile.git
	cd opensmile
	bash build.sh
	cd ..
	
	cd .. 
fi

#Build
mkdir build
cd build
cmake ..
make


