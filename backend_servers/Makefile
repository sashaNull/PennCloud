TARGETS = server

all: $(TARGETS)

server: server.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
