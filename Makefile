tests: tests.cpp include/bev/string_view.hpp
	g++ -std=c++17 -Iinclude/ -o $@ $<

