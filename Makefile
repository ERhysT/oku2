CC=cc
LIBS=-lgpiod
INCLUDE=-I./src
CFLAGS= -Wall -Wextra -Wfatal-errors -g3 -DDEBUG

TARGET=oku
OBJ=oku.o book.o epd.o unifont.o gpio.o err.o spi.o
PI_USERNAME=oku
PI_HOSTNAME=pi
PI_DIR=oku
PI_FULL=$(PI_USERNAME)@$(PI_HOSTNAME):$(PI_DIR)

.PHONY: all clean tags sync remote

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
	rm -f $(OBJ) $(TARGET)

tags:
	@etags src/*.c src/*.h

# remote actions
sync: clean tags
	rsync -rav --exclude '.git' -e ssh --delete . $(PI_FULL) -f "- /*.o" -f "- /oku"
remote: sync
	ssh $(PI_USERNAME)@$(PI_HOSTNAME) make -C$(PI_DIR)/
# delete some annoying timewasting rules
Makefile: ;
%.c: ;
