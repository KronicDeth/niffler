#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "erl_nif.h"

enum ElapsedType {
    ELAPSED_EXCEPTION,
    ELAPSED_MILLISECONDS
};

struct Elapsed {
    enum ElapsedType type;
    union {
        ERL_NIF_TERM exception;
        unsigned long milliseconds;
    };
};

enum TimeoutType {
    TIMEOUT_EXCEPTION,
    TIMEOUT_INFINITY,
    TIMEOUT_MILLISECONDS
};

struct Timeout {
    enum TimeoutType type;
    ERL_NIF_TERM term;
    unsigned long milliseconds;
};

// from `erl_term.h`
#define SCHEDULED 0

static ERL_NIF_TERM burrow(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static unsigned int elapsed_milliseconds_to_timeslice_percent(unsigned int elapsed_milliseconds);
static ERL_NIF_TERM find_treasure(ErlNifEnv* env, struct Timeout timeout, unsigned long elapsed_milliseconds);
static ERL_NIF_TERM find_treasures(ErlNifEnv* env, struct Timeout timeout, unsigned long elapsed_milliseconds);
static struct Elapsed get_elapsed(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static struct Timeout get_timeout(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static bool is_infinity(ErlNifEnv* env, ERL_NIF_TERM term);
static bool is_ok(ErlNifEnv* env, ERL_NIF_TERM term);
static ERL_NIF_TERM make_argument_error(ErlNifEnv* env, const char *message);
static ERL_NIF_TERM make_error_timeout(ErlNifEnv* env);
static ERL_NIF_TERM make_string(ErlNifEnv* env, const char* c_string);
static ERL_NIF_TERM make_treasure(ErlNifEnv* env, const char* name, int value);
static unsigned int measure_elapsed_milliseconds(void (*fun_ptr)());
static ERL_NIF_TERM raise_argument_error(ErlNifEnv* env, const char *message);
static ERL_NIF_TERM schedule_burrow(ErlNifEnv* env, ERL_NIF_TERM timeout, unsigned long elapsed_milliseconds);
static bool timeout_is_exceeded(ErlNifEnv* env, struct Timeout timeout, unsigned long elapsed_milliseconds);
static void work(void *ignored);

static ERL_NIF_TERM burrow(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct Timeout timeout = get_timeout(env, argc, argv);
    ERL_NIF_TERM result;

    if (timeout.type == TIMEOUT_EXCEPTION) {
        result = timeout.term;
    } else {
        struct Elapsed elapsed = get_elapsed(env, argc, argv);

        if (elapsed.type == ELAPSED_EXCEPTION) {
            result = elapsed.exception;
        } else {
            assert(elapsed.type == ELAPSED_MILLISECONDS);
            result = find_treasures(env, timeout, elapsed.milliseconds);
        }
    }

    return result;
}

#define TIMESLICE_MILLISECONDS 1
#define MICROSECONDS_PER_MILLISECOND 1000
#define TIMESLICE_MICROSECONDS (TIMESLICE_MILLISECONDS * MICROSECONDS_PER_MILLISECOND)
#define MILLISECONDS_PER_SECOND 1000
#define FOUND_TREASURE_ELAPSED_MILLISECONDS (5 * MILLISECONDS_PER_SECOND)
#define TIMESLICE_EXHAUSTED 1
#define PERCENT_PER_MILLISECOND 100

static unsigned int elapsed_milliseconds_to_timeslice_percent(unsigned int elapsed_milliseconds) {
    unsigned int percent = elapsed_milliseconds / PERCENT_PER_MILLISECOND;

    if (percent <= 0) {
      // must consume some percentage
      percent = 1;
    } else if (100 < percent) {
      // can only consume 100% max
      percent = 100;
    }

    return percent;
}

static ERL_NIF_TERM find_treasure(ErlNifEnv* env, struct Timeout timeout, unsigned long elapsed_milliseconds) {
   ERL_NIF_TERM result;

   while (true) {
       unsigned int work_elapsed_milliseconds = measure_elapsed_milliseconds(work);

       elapsed_milliseconds += work_elapsed_milliseconds;

       unsigned int percent = elapsed_milliseconds_to_timeslice_percent(work_elapsed_milliseconds);
       int exhausted = enif_consume_timeslice(env, percent);

       if (FOUND_TREASURE_ELAPSED_MILLISECONDS <= elapsed_milliseconds) {
           result = enif_make_tuple2(env, enif_make_atom(env, "ok"), make_treasure(env, "vial", 0));
           break;
       } else {
           if (timeout_is_exceeded(env, timeout, elapsed_milliseconds)) {
               result = make_error_timeout(env);
               break;
           } else if (exhausted == TIMESLICE_EXHAUSTED) {
               assert(timeout.type != TIMEOUT_EXCEPTION);
               result = schedule_burrow(env, timeout.term, elapsed_milliseconds);
               break;
           }
       }
   }

   return result;
}

static ERL_NIF_TERM find_treasures(ErlNifEnv* env, struct Timeout timeout, unsigned long elapsed_milliseconds) {
    ERL_NIF_TERM find_treasure_result = find_treasure(env, timeout, elapsed_milliseconds);
    int tuple_arity;
    const ERL_NIF_TERM* tuple_elements;
    ERL_NIF_TERM result;

    if (find_treasure_result == SCHEDULED) {
      result = find_treasure_result;
    } else if (enif_get_tuple(env, find_treasure_result, &tuple_arity, &tuple_elements) && tuple_arity == 2 &&
                   is_ok(env, tuple_elements[0])) {
        ERL_NIF_TERM treasure_list = enif_make_list1(env, tuple_elements[1]);
        result = enif_make_tuple2(env, tuple_elements[0], treasure_list);
    } else {
        result = find_treasure_result;
    }

    return result;
}

static struct Elapsed get_elapsed(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct Elapsed elapsed;

    if (2 <= argc) {
        if (enif_get_ulong(env, argv[1], &elapsed.milliseconds)) {
          elapsed.type = ELAPSED_MILLISECONDS;
        } else {
          elapsed.type = ELAPSED_EXCEPTION;
          elapsed.exception = raise_argument_error(env, "elapsed must be t:integer/0");
        }
    } else {
        elapsed.type = ELAPSED_MILLISECONDS;
        elapsed.milliseconds = 0;
    }

    return elapsed;
}

#define INFINITY_STRING "infinity"

static struct Timeout get_timeout(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct Timeout timeout;

    if (1 <= argc) {
        ERL_NIF_TERM term = argv[0];

        if (is_infinity(env, term)) {
            timeout.type = TIMEOUT_INFINITY;
            timeout.term = term;
        } else if (enif_get_ulong(env, term, &timeout.milliseconds)) {
            timeout.type = TIMEOUT_MILLISECONDS;
            timeout.term = term;
        } else {
            timeout.type = TIMEOUT_EXCEPTION;
            timeout.term = raise_argument_error(env, "timeout must be t:timeout/0");
        }
    } else {
        timeout.type = TIMEOUT_INFINITY;
        timeout.term = enif_make_atom(env, INFINITY_STRING);
    }

    return timeout;
}

static bool is_infinity(ErlNifEnv* env, ERL_NIF_TERM term) {
    char string[] = INFINITY_STRING;

    return (enif_get_atom(env, term, string, sizeof(string)/sizeof(string[0]), ERL_NIF_LATIN1) &&
                strcmp(string, INFINITY_STRING) == 0);
}

#define OK_STRING "ok"

static bool is_ok(ErlNifEnv* env, ERL_NIF_TERM term) {
    char string[] = OK_STRING;

    return (enif_get_atom(env, term, string, sizeof(string)/sizeof(string[0]), ERL_NIF_LATIN1) &&
                strcmp(string, OK_STRING) == 0);
}

static ERL_NIF_TERM make_argument_error(ErlNifEnv* env, const char *message) {
    ERL_NIF_TERM map;

    ERL_NIF_TERM __struct__ = enif_make_atom(env, "__struct__");
    ERL_NIF_TERM __exception__ = enif_make_atom(env, "__exception__");
    ERL_NIF_TERM message_key = enif_make_atom(env, "message");
    ERL_NIF_TERM keys[] = {__struct__, __exception__, message_key};

    ERL_NIF_TERM argument_error = enif_make_atom(env, "Elixir.ArgumentError");
    ERL_NIF_TERM true_atom = enif_make_atom(env, "true");
    ERL_NIF_TERM message_value = make_string(env, message);
    ERL_NIF_TERM values[] = {argument_error, true_atom, message_value};

    enif_make_map_from_arrays(env, keys, values, 3, &map);

    return map;
}

static ERL_NIF_TERM make_error_timeout(ErlNifEnv* env) {
    return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "timeout"));
}

static ERL_NIF_TERM make_string(ErlNifEnv* env, const char*c_string) {
    size_t bytes = strlen(c_string);
    ERL_NIF_TERM string;
    unsigned char *string_data = enif_make_new_binary(env, bytes, &string);

    strcpy((char *) string_data, c_string);

    return string;
}

static ERL_NIF_TERM make_treasure(ErlNifEnv* env, const char* name, int value) {
    ERL_NIF_TERM map;

    ERL_NIF_TERM __struct__ = enif_make_atom(env, "__struct__");
    ERL_NIF_TERM name_key = enif_make_atom(env, "name");
    ERL_NIF_TERM value_key = enif_make_atom(env, "value");
    ERL_NIF_TERM keys[] = {__struct__, name_key, value_key};

    ERL_NIF_TERM elixir_niffler_treasure = enif_make_atom(env, "Elixir.Niffler.Treasure");
    ERL_NIF_TERM name_value = make_string(env, name);
    ERL_NIF_TERM value_value = enif_make_int(env, value);
    ERL_NIF_TERM values[] = {elixir_niffler_treasure, name_value, value_value};

    enif_make_map_from_arrays(env, keys, values, 3, &map);

    return map;
}

static unsigned int measure_elapsed_milliseconds(void (*fun_ptr)()) {
    struct timeval start, stop, difference;
    gettimeofday(&start, NULL);

    fun_ptr();

    gettimeofday(&stop, NULL);
    timersub(&stop, &start, &difference);

    return difference.tv_sec * MILLISECONDS_PER_SECOND + difference.tv_usec / MICROSECONDS_PER_MILLISECOND;
}

static ERL_NIF_TERM raise_argument_error(ErlNifEnv* env, const char* message) {
    return enif_raise_exception(env, make_argument_error(env, message));
}

#define NORMAL_NIF 0

static ERL_NIF_TERM schedule_burrow(ErlNifEnv* env, ERL_NIF_TERM timeout, unsigned long elapsed_milliseconds) {
    ERL_NIF_TERM elapsed = enif_make_ulong(env, elapsed_milliseconds);
    const int argc = 2;
    ERL_NIF_TERM argv[argc] = {timeout, elapsed};

    return enif_schedule_nif(env, "burrow", NORMAL_NIF, burrow, argc, argv);
}

static bool timeout_is_exceeded(ErlNifEnv* env, struct Timeout timeout, unsigned long elapsed_milliseconds) {
    bool exceeded;

    if (timeout.type == TIMEOUT_INFINITY) {
        exceeded = false;
    } else {
        assert(timeout.type == TIMEOUT_MILLISECONDS);

        exceeded = (timeout.milliseconds < elapsed_milliseconds);
    }

    return exceeded;
}

static void work(void *ignored) {
    usleep(TIMESLICE_MICROSECONDS);
}

static ErlNifFunc nif_funcs[] = {
    {"burrow", 2, burrow}
};

ERL_NIF_INIT(Elixir.Niffler, nif_funcs, NULL, NULL, NULL, NULL)
