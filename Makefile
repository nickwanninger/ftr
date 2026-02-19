main: main.cpp ftr.c ftr.h
	clang -o ftr.o -c ftr.c -O3
	clang++ -o main ftr.o main.cpp -lpthread -O3
	clang++ -o main_baseline -DFTR_NO_TRACE=1 ftr.o main.cpp -lpthread -O3