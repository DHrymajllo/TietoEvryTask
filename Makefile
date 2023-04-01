all: compile
	
compile: 
	g++ -std=c++17 -pthread TietoEvryTask_Dorian_H.cpp -o specific_grep
