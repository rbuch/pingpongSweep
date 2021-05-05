CHARMC?=../../../bin/charmc $(OPTS)

OBJS = pingpong.o

all:	pingpong

pingpong: $(OBJS)
	$(CHARMC) -language charm++ -o pingpong $(OBJS)

pingpong.prj: $(OBJS)
	$(CHARMC) -tracemode projections -language charm++ -o pingpong.prj $(OBJS)

cifiles: pingpong.ci
	$(CHARMC)  pingpong.ci
	touch cifiles

clean:
	rm -f *.decl.h *.def.h conv-host *.o pingpong charmrun cifiles pingpong.exe pingpong.pdb pingpong.ilk

pingpong.o: pingpong.C cifiles
	$(CHARMC) -I$(SRC)/conv-core pingpong.C

test: all
	./charmrun +p2 ./pingpong

