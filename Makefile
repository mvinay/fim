install: src/fim.cpp
	     mkdir -p bin
	     g++ -std=c++11 -o ./bin/fim src/fim.cpp -lssl -lcrypto

clean: 
		rm -rf bin
