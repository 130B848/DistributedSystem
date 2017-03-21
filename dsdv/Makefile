all: util.o dsdv.o main.o 
	g++ --std=c++11 -pthread util.o dsdv.o main.o -o dsdv

main.o: main.cpp dsdv.h	
	g++ --std=c++11 -c main.cpp

dsdv.o: dsdv.cpp dsdv.h util.h
	g++ --std=c++11 -c dsdv.cpp
	
util.o: util.cpp util.h
	g++ --std=c++11 -c util.cpp
	
clean: 
	rm -f util.o dsdv.o main.o dsdv

handin:
	tar -cvzf [DS]lab2_5140309358.tar.gz ./*
