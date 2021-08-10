OBJS=$(shell ls *.cc | sed 's/\.cc\>/.o/')

libht.a : $(OBJS)
	ar r libht.a $(OBJS)

%.o:%.cc
	g++ -g -o $@ -c $< # -I../comm/ -DHAS_ATTR_API

clean:
	rm -f *.o libht.a
