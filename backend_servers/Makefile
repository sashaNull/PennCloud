TARGETS = server

all: $(TARGETS)

server: server.cpp ../utils/utils.cpp ./utils.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
