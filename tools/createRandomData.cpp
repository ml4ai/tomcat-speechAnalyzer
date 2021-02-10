#include <iostream>
#include <fstream> //ofstream
#include <stdlib.h> //rand

const size_t FILE_SIZE = 512; 

int main(){
	int count = 0;

	std::ofstream file;
	file.open("random.dat", std::ios::out|std::ios::binary);
	while(count < FILE_SIZE){
		char data = rand() % 256; 
		file.write(&data, 1);
		count++;
	}
	file.close();
	return 0;	
}
