/**************************
 * sha-3 (keccak) hash    *
 * written by: ni-richard *
 **************************/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <lua5.3/lua.h>
#include <lua5.3/lauxlib.h>

typedef struct keccak_t {
  uint32_t lock, pos;
  union {
    uint8_t state_b[200];
    uint64_t state_q[25];
  };
} keccak_t;

static void keccak_sum(uint64_t st[25]);
static void keccak_work(keccak_t *hash, const void *block, size_t size);
static void keccak_xof(keccak_t *hash);
static void keccak_out(keccak_t *hash, uint8_t *block, size_t size);

static int lua_init(lua_State *L);
static int lua_work(lua_State *L);
static int lua_xof(lua_State *L);
static int lua_out(lua_State *L);
static int lua_data(lua_State *L);
static int lua_file(lua_State *L);
static int lua_dest(lua_State *L);

const luaL_Reg keccaklib[] = {
  {"init", lua_init},
  {"work", lua_work},
  {"xof", lua_xof},
  {"out", lua_out},
  {"data", lua_data},
  {"file", lua_file},
  {NULL, NULL}
};

static const uint64_t keccak_rndc[24] = {
  0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
  0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
  0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
  0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
  0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
  0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
  0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
  0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

static const uint8_t keccak_rotc[24] = {
  1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
  27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
};

static const uint8_t keccak_piln[24] = {
  10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
  15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
};

static uint64_t rotl64(uint64_t x, uint64_t y) {
  return (x << y) | (x >> (64 - y));
}

static void keccak_sum(uint64_t st[25]) {
  uint32_t n, i, k;
  uint64_t bc[5], bin, tmp;

  for(n = 0; n < 24; n++) {
    for(i = 0; i < 5; i++)
      bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

    for(i = 0; i < 5; i++) {
      bin = bc[(i + 4) % 5] ^ rotl64(bc[(i + 1) % 5], 1);
      for(k = 0; k < 25; k += 5)
        st[i + k] ^= bin;
    }

    bin = st[1];
    for(i = 0; i < 24; i++) {
      k = keccak_piln[i];
      tmp = st[k];
      st[k] = rotl64(bin, keccak_rotc[i]);
      bin = tmp;
    }

    for(k = 0; k < 25; k += 5) {
      for(i = 0; i < 5; i++)
        bc[i] = st[i + k];
      for(i = 0; i < 5; i++)
        st[i + k] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
    }

    st[0] ^= keccak_rndc[n];
  }
}

static void keccak_work(keccak_t *hash, const void *block, size_t size) {
  uint32_t i, k;

  k = hash->pos;
  for(i = 0; i < size; i++) {
    hash->state_b[k++] ^= ((const uint8_t *)block)[i];
    if(k == 136)
      keccak_sum(hash->state_q), k = 0;
  }
  hash->pos = k;
}

static void keccak_xof(keccak_t *hash) {
  hash->state_b[hash->pos] ^= 0x1f;
  hash->state_b[135] ^= 0x80;
  keccak_sum(hash->state_q);
  hash->pos = 0;
}

static void keccak_out(keccak_t *hash, uint8_t *block, size_t size) {
  uint32_t i, k;

  k = hash->pos;
  for(i = 0; i < size; i++) {
    if(k == 136)
      keccak_sum(hash->state_q), k = 0;
    block[i] = hash->state_b[k++];
  }
  hash->pos = k;
}

static int lua_init(lua_State *L) {
  keccak_t *sponge;

  if(lua_gettop(L) == 1) {
    sponge = luaL_checkudata(L, 1, "keccak");
    memset(sponge, 0, sizeof(keccak_t));
    return 0;
  }
  else {
    sponge = lua_newuserdata(L, sizeof(keccak_t));
    memset(sponge, 0, sizeof(keccak_t));
    luaL_setmetatable(L, "keccak");
    return 1;
  }
}

static int lua_work(lua_State *L) {
  size_t length;
  keccak_t *sponge = luaL_checkudata(L, 1, "keccak");
  const char *block = luaL_checklstring(L, 2, &length);

  if(sponge->lock != 0)
    return luaL_error(L, "Invalid state.");
  keccak_work(sponge, block, length);
  return 0;
}

static int lua_xof(lua_State *L) {
  keccak_t *sponge = luaL_checkudata(L, 1, "keccak");

  if(sponge->lock != 0)
    return luaL_error(L, "Invalid state.");
  sponge->lock = 1;
  keccak_xof(sponge);
  return 0;
}

static int lua_out(lua_State *L) {
  keccak_t *sponge = luaL_checkudata(L, 1, "keccak");
  size_t length = luaL_checkinteger(L, 2);
  char *stream = (char *)malloc(length);

  if(sponge->lock != 1)
    return luaL_error(L, "Invalid state.");
  keccak_out(sponge, (uint8_t *)stream, length);
  lua_pushlstring(L, stream, length);
  free(stream);
  return 1;
}

static int lua_data(lua_State *L) {
  keccak_t sponge = { 0 };
  size_t length;
  const char *data = luaL_checklstring(L, 1, &length);
  size_t size = luaL_checkinteger(L, 2);
  char *stream = (char *)malloc(size);

  keccak_work(&sponge, (uint8_t *)data, length);
  keccak_xof(&sponge);
  keccak_out(&sponge, (uint8_t *)stream, size);
  lua_pushlstring(L, stream, size);
  free(stream);
  return 1;
}

static int lua_file(lua_State *L) {
  char *stream;
  keccak_t sponge = { 0 };
  const char *path = luaL_checkstring(L, 1);
  size_t length, size = luaL_checkinteger(L, 2);
  FILE *file = fopen(path, "rb");
  uint8_t block[4096];

  if(!file)
    return luaL_error(L, "Could not open file.");
  stream = (char *)malloc(size);
  while((length = fread(block, 1, 4096, file)))
    keccak_work(&sponge, block, length);
  fclose(file);

  keccak_xof(&sponge);
  keccak_out(&sponge, (uint8_t *)stream, size);
  lua_pushlstring(L, stream, size);
  free(stream);
  return 1;
}

static int lua_dest(lua_State *L) {
  keccak_t *sponge = luaL_checkudata(L, 1, "keccak");

  memset(sponge, 0, sizeof(keccak_t));
  return 0;
}

int luaopen_keccak(lua_State *L) {
  luaL_newlib(L, keccaklib);
  luaL_newmetatable(L, "keccak");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, lua_dest);
  lua_setfield(L, -2, "__gc");
  luaL_setfuncs(L, keccaklib, 0);
  lua_pop(L, 1);
  return 1;
}
