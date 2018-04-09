all: nameserver.h nameserver.cpp DNSHeader.h DNSQuestion.h DNSRecord.h graph.h graph.cpp
	g++ -o nameserver -std=c++11 nameserver.cpp graph.cpp
clean:
	rm nameserver log.txt