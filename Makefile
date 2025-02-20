SOURCE=*.c
PROGRAM=program
EXE_NAME=$(PROGRAM).o
ARGS=
BUILD_DIR=build/
EXE_PATH=$(BUILD_DIR)$(EXE_NAME)
DEFAULT_FLAGS=
STRICT_FLAGS= $(DEFAULT_FLAGS) -std=c99 -Wall -pedantic -Wextra
DEBUG_FLAGS= $(STRICT_FLAGS) -g -o0

default:
	gcc $(SOURCE) $(DEFAULT_FLAGS) -o $(EXE_PATH)
	make post-build
                                     
strict:                              
	bear -- gcc $(SOURCE) $(STRICT_FLAGS) -o $(EXE_PATH)
	make post-build
                    
debug:              
	gcc $(SOURCE) $(DEBUG_FLAGS) -o $(EXE_PATH)
	make post-build

.ONESHELL:
run:
	 cd $(BUILD_DIR); ./$(EXE_NAME) $(ARGS)

.ONESHELL:
run-tui:
	cd $(BUILD_DIR); bash start.sh

andrun:
	make default
	make run

gdb:
	gdb $(EXE_PATH) $(ARGS)

valgrind:
	valgrind -s --leak-check=yes --track-origins=yes $(EXE_PATH) $(ARGS)

clean:
	rm -f $(EXE_PATH)
	rm -f $(BUILD_DIR)*.sh
	rm -f compile_commands.json

post-build:
	mkdir $(BUILD_DIR)
	sudo setcap 'CAP_NET_BIND_SERVICE=ep' $(EXE_PATH)
	cp bash_tui/* $(BUILD_DIR)
	echo exe_name=$(EXE_NAME) > $(BUILD_DIR)exe_name.sh

