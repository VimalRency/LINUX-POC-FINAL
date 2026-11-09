CC := gcc
CXX := g++
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
          -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Werror -O2
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS ?=
LDLIBS := -pthread
GTEST_LIBS := -lgtest -lgtest_main -pthread
MISRA_ADDON ?= misra.py

CORE_SOURCES := src/supervisor.c src/config.c src/logger.c
SOURCES := src/main.c $(CORE_SOURCES) src/procmon.c
OBJECTS := $(SOURCES:.c=.o)
CORE_OBJECTS := $(CORE_SOURCES:.c=.o)
TARGET := process_supervisor
GTEST_TARGET := tests/test_config

.PHONY: all clean run debug test test-gtest test-integration test-restart test-startup-errors \
        cppcheck valgrind helgrind coverage misra

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(GTEST_TARGET): tests/test_config.cpp $(CORE_OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(GTEST_LIBS)

test: test-gtest test-integration test-restart test-startup-errors

test-gtest: $(GTEST_TARGET)
	./$(GTEST_TARGET)

test-integration: $(TARGET)
	bash tests/integration_dependencies.sh

test-restart: $(TARGET)
	bash tests/integration_restart.sh

test-startup-errors: $(TARGET)
	bash tests/integration_startup_errors.sh

run: $(TARGET)
	mkdir -p logs
	./$(TARGET) config/services.conf

cppcheck:
	cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
		--inline-suppr --suppress=missingIncludeSystem -I include src

valgrind: $(TARGET)
	mkdir -p logs
	rm -f logs/valgrind.log
	valgrind --tool=memcheck \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--log-file=logs/valgrind.log \
		./$(TARGET) config/services.conf & \
	vg_pid=$$!; \
	sleep 8; \
	kill -INT $$vg_pid 2>/dev/null || true; \
	wait $$vg_pid || true

helgrind: $(TARGET)
	mkdir -p logs
	rm -f logs/helgrind.log
	valgrind --tool=helgrind \
		--history-level=approx \
		--log-file=logs/helgrind.log \
		./$(TARGET) config/services.conf & \
	vg_pid=$$!; \
	sleep 8; \
	kill -INT $$vg_pid 2>/dev/null || true; \
	wait $$vg_pid || true

coverage:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -O0 -g --coverage" \
		CXXFLAGS="-std=c++17 -Wall -Wextra -Wpedantic -O0 -g --coverage" \
		LDFLAGS="--coverage" all tests/test_config
	./tests/test_config
	bash tests/integration_dependencies.sh
	bash tests/integration_restart.sh
	bash tests/integration_startup_errors.sh
	gcov -b -c -o src src/main.c src/supervisor.c src/config.c src/logger.c src/procmon.c

misra:
	mkdir -p logs
	bash -o pipefail -c 'cppcheck \
		--enable=warning,style,performance,portability \
		--std=c11 \
		--platform=unix64 \
		--library=posix \
		--addon=$(MISRA_ADDON) \
		--inline-suppr \
		-DPOLLIN=1 \
		-DEINTR=4 \
		-DEAGAIN=11 \
		-DEWOULDBLOCK=11 \
		-DEEXIST=17 \
		-I include \
		src 2>&1 | tee logs/misra_diagnostics.log'
debug: CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
                 -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -g3 -O0

debug: clean $(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(GTEST_TARGET) src/*.gcda src/*.gcno *.gcov
