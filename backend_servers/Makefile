TARGETS = server

all: $(TARGETS)

server: server.cpp ../utils/utils.cpp ./utils.cpp
	g++-10 $^ -std=c++20 -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
