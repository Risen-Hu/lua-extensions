/**************************
 * file system operations *
 * written by: ni-richard *
 **************************/

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <sys/stat.h>

#include <lua5.3/lua.h>
#include <lua5.3/lauxlib.h>

typedef struct direntry_t {
  char *path;
  DIR *stream;
} direntry_t;

typedef struct dirlist_t {
  direntry_t *entries;
  size_t length, size;
} dirlist_t;

static int dirlist_init(lua_State *L, lua_CFunction func);
static void dirlist_append(dirlist_t *list, const char *path);
static int dirlist_visit(lua_State *L, bool recursive);
static int dirlist_dest(lua_State *L);

static int lua_listdir(lua_State *L);
static int lua_listiter(lua_State *L);
static int lua_treedir(lua_State *L);
static int lua_treeiter(lua_State *L);
static int lua_stat(lua_State *L);
static int lua_touch(lua_State *L);
static int lua_mkdir(lua_State *L);
static int lua_chmod(lua_State *L);
static int lua_mirror(lua_State *L);
static int lua_rename(lua_State *L);
static int lua_delete(lua_State *L);

static const luaL_Reg fslib[] = {
  {"list", lua_listdir},
  {"tree", lua_treedir},
  {"stat", lua_stat},
  {"touch", lua_touch},
  {"mkdir", lua_mkdir},
  {"rename", lua_rename},
  {"delete", lua_delete},
  {"chmod", lua_chmod},
  {"mirror", lua_mirror},
  {NULL, NULL}
};

static int dirlist_init(lua_State *L, lua_CFunction func) {
  dirlist_t *list;
  struct stat props;
  const char *path = luaL_checkstring(L, 1);

  if(stat(path, &props) || !S_ISDIR(props.st_mode))
    return luaL_error(L, "Could not open directory.");

  list = lua_newuserdata(L, sizeof(dirlist_t));
  memset(list, 0, sizeof(dirlist_t));
  list->entries = (direntry_t *)malloc(sizeof(direntry_t));
  list->size = 1;
  luaL_setmetatable(L, "folder");
  dirlist_append(list, path);
  lua_pushcclosure(L, func, 1);
  return 1;
}

static void dirlist_append(dirlist_t *list, const char *path) {
  DIR *stream;
  char *new_path;

  if((stream = opendir(path)) == NULL)
    return;

  new_path = (char *)malloc(strlen(path) + NAME_MAX + 2);
  sprintf(new_path, "%s/", path);
  if(++list->length >= list->size) {
    list->size *= 2;
    list->entries =
      (direntry_t *)realloc(list->entries, sizeof(direntry_t) * list->size);
  }
  list->entries[list->length - 1].stream = stream;
  list->entries[list->length - 1].path = new_path;
}

static int dirlist_visit(lua_State *L, bool recursive) {
  size_t i, old_len;
  struct dirent *node;
  direntry_t *entry;
  dirlist_t *list = luaL_checkudata(L, lua_upvalueindex(1), "folder");

  while(list->length) {
    i = list->length - 1, entry = &list->entries[i];
    if((node = readdir(entry->stream)) != NULL) {
      if(!strcmp(".", node->d_name) || !strcmp("..", node->d_name))
        continue;
      old_len = strlen(entry->path);
      strcat(entry->path, node->d_name);
      lua_pushstring(L, entry->path);
      if(recursive && node->d_type == DT_DIR)
        dirlist_append(list, entry->path);
      list->entries[i].path[old_len] = '\0';
      return 1;
    }
    else {
      closedir(entry->stream);
      free(entry->path);
      list->length--;
    }
  }
  return 0;
}

static int dirlist_dest(lua_State *L) {
  size_t i;
  dirlist_t *list = luaL_checkudata(L, 1, "folder");

  for(i = 0; i < list->length; i++) {
    closedir(list->entries[i].stream);
    free(list->entries[i].path);
  }
  free(list->entries);
  return 0;
}

static int lua_listdir(lua_State *L) {
  return dirlist_init(L, lua_listiter);
}

static int lua_listiter(lua_State *L) {
  return dirlist_visit(L, false);
}

static int lua_treedir(lua_State *L) {
  return dirlist_init(L, lua_treeiter);
}

static int lua_treeiter(lua_State *L) {
  return dirlist_visit(L, true);
}

static int lua_stat(lua_State *L) {
  struct stat props;
  const char *path = luaL_checkstring(L, 1);

  if(stat(path, &props))
    return 0;

  lua_newtable(L);
  lua_pushnumber(L, props.st_mtime);
  lua_setfield(L, -2, "date");
  lua_pushnumber(L, props.st_size);
  lua_setfield(L, -2, "size");
  if(S_ISREG(props.st_mode))
    lua_pushstring(L, "file");
  else
    lua_pushstring(L, "folder");
  lua_setfield(L, -2, "type");
  return 1;
}

static int lua_touch(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int fd = open(path, O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK, 0666);

  if(fd < 0) {
    lua_pushboolean(L, false);
    return 1;
  }

  close(fd);
  lua_pushboolean(L, !utime(path, NULL));
  return 1;
}

static int lua_mkdir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  mode_t mode = luaL_checkinteger(L, 2);

  lua_pushboolean(L, !mkdir(path, mode));
  return 1;
}

static int lua_chmod(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  mode_t mode = luaL_checkinteger(L, 2);

  lua_pushboolean(L, !chmod(path, mode));
  return 1;
}

static int lua_mirror(lua_State *L) {
  size_t length;
  char block[4096];
  struct stat props;
  struct utimbuf date;
  const char *old_name = luaL_checkstring(L, 1);
  const char *new_name = luaL_checkstring(L, 2);
  FILE *old_file = fopen(old_name, "rb"), *new_file = fopen(new_name, "wb");

  if(stat(old_name, &props) || !old_file || !new_file) {
    if(old_file)
      fclose(old_file);
    if(new_file)
      fclose(new_file);
    return luaL_error(L, "Could not open input/output file.");
  }

  while((length = fread(block, 1, 4096, old_file)))
    fwrite(block, 1, length, new_file);
  fclose(old_file), fclose(new_file);

  date.actime = time(NULL);
  date.modtime = props.st_mtime;
  if(utime(new_name, &date))
    return luaL_error(L, "Failed to update the modification timestamp.");
  if(chmod(new_name, props.st_mode))
    return luaL_error(L, "Failed to set file permissions.");
  if(chown(new_name, props.st_uid, props.st_gid))
    return luaL_error(L, "Failed to set owner of the file.");
  return 0;
}

static int lua_rename(lua_State *L) {
  const char *old_name = luaL_checkstring(L, 1);
  const char *new_name = luaL_checkstring(L, 2);

  lua_pushboolean(L, !rename(old_name, new_name));
  return 1;
}

static int lua_delete(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);

  lua_pushboolean(L, !remove(path));
  return 1;
}

int luaopen_filesystem(lua_State *L) {
  luaL_newmetatable(L, "folder");
  lua_pushcfunction(L, dirlist_dest);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);

  luaL_newlib(L, fslib);
  luaL_newmetatable(L, "filesystem");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, fslib, 0);
  lua_pop(L, 1);
  return 1;
}
