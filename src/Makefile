top = ..

bin = $(top)/bin/get

src = $(top)/src/main.cpp

all: get

include $(config)

get:
	@mkdir -p $(top)/bin
	g++ $(flags) $(EPICS_INCLUDE) $(EPICS_LIBRARY) $(src) -o $(bin)

.PHONY: clean
clean:
	@printf "Cleaning binary...\n"
	@mkdir -p $(top)/bin
	rm -rf $(bin)
