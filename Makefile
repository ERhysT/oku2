CC=cc
LIBS=-lgpiod
INCLUDE=-I./src
CFLAGS= -Wall -Wextra -Wfatal-errors -g3 -DDEBUG

TARGET=oku
OBJ=oku.o epd.o gpio.o err.o spi.o

PI_USERNAME=oku
PI_HOSTNAME=pi
PI_DIR=oku
PI_FULL=$(PI_USERNAME)@$(PI_HOSTNAME):$(PI_DIR)

.PHONY: all ball clean tags sync build run

ifeq '$(USER)' '$(PI_USERNAME)'
all: $(TARGET)
else
all: sync
endif

$(TARGET): $(OBJ) 
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@ $(LIBS)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@ $(LIBS)

clean:
	rm -f *.o
	rm -f oku

tags:
	etags src/*.c src/*.h

piscope: 
	./util/piscope.sh $(PI_HOSTNAME)

# remote actions
sync: clean tags
	rsync -rav --exclude '.git' -e ssh --delete . $(PI_FULL)
build: sync
	ssh -t $(PI_USERNAME)@$(PI_HOSTNAME) "cd $(PI_DIR) && make $(TARGET)"
run: build
	-ssh -t $(PI_USERNAME)@$(PI_HOSTNAME) "$(PI_DIR)/$(TARGET)"

# delete some annoying timewasting rules
Makefile: ;
%.c: ;
