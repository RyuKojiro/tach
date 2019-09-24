PROGNAME=tach
PROG=../$(PROGNAME)

test: 1-fifteen-columns 2-pty-expected 4b-zero-lines 4c-one-line 4d-n-lines 5-newline-io
	@if which expect > /dev/null; \
	then \
		echo "Done running tests."; \
	else \
		echo "Not running tests, since expect was not found in PATH."; \
	fi

.SUFFIXES: .exp

.exp: $(PROG)
	@if which expect > /dev/null; \
	then \
		expect $<; \
	fi

$(PROG):
	make -C .. $(PROGNAME)

.PHONY: test

.POSIX:
