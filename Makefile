CFLAGS = -g -O3 -Wall -Werror

ERLANG_PATH =  $(shell erl -eval 'io:format("~s", [lists:concat([code:root_dir(), "/erts-", erlang:system_info(version), "/include"])])' -s init stop -noshell)
CFLAGS += -I$(ERLANG_PATH)
CFLAGS += -Ic_src

LIB_NAME = priv/niffler.so
CFLAGS += -fPIC

ifeq ($(shell uname),Darwin)
	LDFLAGS += -dynamiclib -undefined dynamic_lookup
endif

NIF_SRC=\
	c_src/niffler.c

all: $(LIB_NAME)

$(LIB_NAME): $(NIF_SRC)
	mkdir -p priv
	$(CC) $(CFLAGS) -shared $(LDFLAGS) $^ -o $@

clean:
	rm -f $(LIB_NAME)

.PHONY: all clean
