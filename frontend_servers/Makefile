TARGETS = server

all: $(TARGETS)

server: server.cpp ../utils/utils.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
