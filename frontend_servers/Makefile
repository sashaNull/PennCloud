TARGETS = frontend

all: $(TARGETS)

frontend: server.cpp ../utils/utils.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
