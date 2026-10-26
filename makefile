gobang: gobang.cc logger.hpp util.hpp db.hpp online.hpp gameroom.hpp 
	g++ -g -std=c++11 $^ -o $@ -L/usr/lib64/mysql -lmysqlclient -ljsoncpp -lpthread