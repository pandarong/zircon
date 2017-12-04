CC=clang++
FORMATTER=../../../scripts/clang-fmt
INCLUDES=./include
FILES_TO_FORMAT=./*.cpp ./include/mdns/*.h ./examples/*.cpp
EXAMPLE=mdns_server

build-example:
	$(CC) -I $(INCLUDES) examples/main.cpp mdns.cpp -o $(EXAMPLE)

format:
	$(FORMATTER) $(FILES_TO_FORMAT)

clean: 
	rm include/mdns/*.gch $(EXAMPLE)
