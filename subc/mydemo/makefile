.PHONY: all demo clean
SRC = $(wildcard *.c)
EXE = $(SRC:.c=)
LST = $(SRC:.c=.lst)

CFLAGS = -Wno-return-stack-address

all : $(EXE)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

spotless : clean
	rm -f *~

clean : .FORCE
	rm -rf $(EXE) *.dSYM

listings : $(LST)

# ASan を有効にしてコンパイル
asan_build: clean
	@echo "=== Compiling with AddressSanitizer ==="
	@for src in $(SRC); do \
		exe=$${src%.c}; \
		echo "Compiling $$src to $$exe with ASan..."; \
		$(CC) $(CFLAGS) -fsanitize=address -g -o $$exe $$src; \
	done

# コンパイル済みの実行ファイルを順番に実行
asan_run:
	@echo "=== Running ASan-compiled executables ===" | tee evalasan.txt
	@for exe in $(EXE); do \
		if [ -x $$exe ]; then \
			echo "Running ASan on $$exe..." | tee -a evalasan.txt; \
			./$$exe 2>&1 | tee -a evalasan.txt; \
			echo "" | tee -a evalasan.txt; \
		fi \
	done

# Valgrind で実行し、結果をファイルとターミナルに出力
demo: all
	@echo "=== Valgrind Execution Results ===" | tee evalvar.txt
	@for exe in $(EXE); do \
		echo "\n--------- $$exe" | tee -a evalvar.txt; \
		valgrind --leak-check=full --show-leak-kinds=all ./$$exe 2>&1 | tee -a evalvar.txt; \
		echo "" | tee -a evalvar.txt; \
	done
%.lst : %.c
	sed '/\/\//,$$!d' < $< > $@

.FORCE :
