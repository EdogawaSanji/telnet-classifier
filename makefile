objects = telnet.o  switch.o

telnet: $(objects)
	g++ -g -o telnet $(objects)
telnet.o: telnet.cpp switch.h
	g++ -g -c telnet.cpp
switch.o: switch.cpp switch.h
	g++ -g -c switch.cpp

.PHONY : clean
clean:
	-rm -rf telnet $(objects)
