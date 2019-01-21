#include <string.h>

#include "erl_nif.h"

static ERL_NIF_TERM burrow(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM find_treasure(ErlNifEnv* env);
static ERL_NIF_TERM find_treasures(ErlNifEnv* env);
static ERL_NIF_TERM make_string(ErlNifEnv* env, const char*c_string);
static ERL_NIF_TERM make_treasure(ErlNifEnv* env, const char* name, int value);

static ERL_NIF_TERM burrow(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ERL_NIF_TERM ok = enif_make_atom(env, "ok");

    return enif_make_tuple2(env, ok, find_treasures(env));
}

static ERL_NIF_TERM find_treasure(ErlNifEnv* env) {
  return make_treasure(env, "vial", 0);
}

static ERL_NIF_TERM find_treasures(ErlNifEnv* env) {
    return enif_make_list1(env, find_treasure(env));
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

static ErlNifFunc nif_funcs[] = {
    {"burrow", 0, burrow}
};

ERL_NIF_INIT(Elixir.Niffler, nif_funcs, NULL, NULL, NULL, NULL)
