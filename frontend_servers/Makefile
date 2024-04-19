TARGETS = server

all: $(TARGETS)

server: server.cpp ../utils/utils.cpp ./backend_communication.cpp ./client_communication.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
