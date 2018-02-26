## -*- mode: makefile -*-

ALL_PLUGINS = \
    libscorep_substrate_print_metrics.so

all: $(ALL_PLUGINS)

libscorep_substrate_print_metrics.so.o: print_metrics.c
	gcc -g -O0 -c -fPIC print_metrics.c   \
            -o libscorep_substrate_print_metrics.so.o  \
            `scorep-config --cppflags`

libscorep_substrate_print_metrics.so: libscorep_substrate_print_metrics.so.o
	gcc -g -O0 -shared -Wl,-soname,libscorep_substrate_print_metrics.so  \
            -o libscorep_substrate_print_metrics.so libscorep_substrate_print_metrics.so.o
            
clean:
	$(RM) libscorep_substrate_print_metrics.so *.o
