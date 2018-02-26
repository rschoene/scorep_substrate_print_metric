ALL_PLUGINS = \
    libscorep_substrate_print_metrics.so

all: $(ALL_PLUGINS)

libscorep_substrate_print_metrics.so.o: print_metrics.c
	gcc -std=c99 -g -c -fPIC print_metrics.c   \
            -o libscorep_substrate_print_metrics.so.o  \
            `scorep-config --cppflags`

libscorep_substrate_print_metrics.so: libscorep_substrate_print_metrics.so.o
	gcc -std=c99 -g -shared -Wl,-soname,libscorep_substrate_print_metrics.so  \
            -o libscorep_substrate_print_metrics.so libscorep_substrate_print_metrics.so.o
            
clean:
	$(RM) libscorep_substrate_print_metrics.so *.o
