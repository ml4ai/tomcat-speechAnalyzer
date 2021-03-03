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

	#JSON
	git clone https://github.com/nlohmann/json.git
	
	#BOOST
	wget -c 'http://sourceforge.net/projects/boost/files/boost/1.75.0/boost_1_75_0.tar.bz2/download'
	tar --bzip2 -xf download
	cd .. 
fi

#Build
mkdir build
cd build
cmake ..
make


