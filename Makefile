GCC ?= gcc
GXX ?= g++
PLUGIN = builtin_lookup_name.so

all: $(PLUGIN)

$(PLUGIN): plugin.c
	$(GXX) -g -Wall -Werror -I`$(GCC) -print-file-name=plugin`/include -fpic -shared -o $@ $<

tester: $(PLUGIN) tester.c
	$(GCC) -O2 -fplugin=./$(PLUGIN) tester.c -o tester

run: tester
	./tester

clean:
	rm -f $(PLUGIN) *.o

.PHONY: clean run all
