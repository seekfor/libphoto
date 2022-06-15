
CC := gcc
AR := ar

CFLAGS := -fPIC -pthread -I./src/include -D_GNU_SOURCE
CFLAGS += -I./include

LDFLAGS += -lpthread -lm -lc -ldl

SRCS += $(wildcard ./source/*.c)

OBJS := $(patsubst %.c,%.o, $(SRCS))


%.o: %.c
	@echo compiling $<
	@$(CC) $(CFLAGS) -o $@ -c $<

all: ./test/testgif ./test/testbmp ./test/testpcx ./test/testtiff ./test/testpng

./test/testgif: $(OBJS) ./test/testgif.o
	@$(CC) -o $@ $(LDFLAGS) $^
	@echo build ./test/testgif OK!
./test/testbmp: $(OBJS) ./test/testbmp.o
	@$(CC) -o $@ $(LDFLAGS) $^
	@echo build ./test/testbmp OK!
./test/testpcx: $(OBJS) ./test/testpcx.o
	@$(CC) -o $@ $(LDFLAGS) $^
	@echo build ./test/testpcx OK!
./test/testtiff: $(OBJS) ./test/testtiff.o
	@$(CC) -o $@ $(LDFLAGS) $^
	@echo build ./test/testtiff OK!
./test/testpng: $(OBJS) ./test/testpng.o
	@$(CC) -o $@ $(LDFLAGS) $^
	@echo build ./test/testpng OK!
clean:
	-@rm -rf $(OBJS) ./test/*.o ./test/testgif ./test/testbmp ./test/testpcx ./test/testtiff ./test/testpng






