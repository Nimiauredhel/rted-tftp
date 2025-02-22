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
	make setcap
                                     
strict:                              
	bear -- gcc $(SOURCE) $(STRICT_FLAGS) -o $(EXE_PATH)
	make post-build
	make setcap
                    
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
	cd $(BUILD_DIR); gdb ./$(EXE_NAME) $(ARGS)

valgrind:
	cd $(BUILD_DIR); valgrind -s --leak-check=yes --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(EXE_NAME) $(ARGS)

clean:
	rm -f $(EXE_PATH)
	rm -f $(BUILD_DIR)*.sh
	rm -f compile_commands.json

setcap:
	sudo setcap 'CAP_NET_BIND_SERVICE=ep' $(EXE_PATH)

post-build:
	mkdir $(BUILD_DIR)
	cp bash_tui/* $(BUILD_DIR)
	echo exe_name=$(EXE_NAME) > $(BUILD_DIR)exe_name.sh

