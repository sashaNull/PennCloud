TARGETS = smtp

# Add -lssl and -lcrypto to link against OpenSSL libraries
LDFLAGS = -lpthread -lssl -lcrypto -g

all: $(TARGETS)

smtp: smtp.cpp ../../utils/utils.cpp ../backend_communication.cpp ../webmail.cpp
	g++ $^ $(LDFLAGS) -o $@

clean::
	rm -fv $(TARGETS) *~