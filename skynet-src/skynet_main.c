#include "skynet_imp.h"
#include "skynet_env.h"
#include "luacompat52.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <signal.h>

static int
optint(const char *key, int opt) {
	const char * str = skynet_getenv(key);
	if (str == NULL) {
		char tmp[20];
		sprintf(tmp,"%d",opt);
		skynet_setenv(key, tmp);
		return opt;
	}
	return strtol(str, NULL, 10);
}


static const char *
optstring(const char *key,const char * opt) {
	const char * str = skynet_getenv(key);
	if (str == NULL) {
		if (opt) {
			skynet_setenv(key, opt);
			opt = skynet_getenv(key);
		}
		return opt;
	}
	return str;
}

static void
_init_env(lua_State *L) {
	lua_pushglobaltable(L);
	lua_pushnil(L);  /* first key */
	while (lua_next(L, -2) != 0) {		// 弹出key并将全局变量表中的全局变量与全局变量值压入lua栈
		int keyt = lua_type(L, -2);		// 获取key类型
		if (keyt != LUA_TSTRING) {
			fprintf(stderr, "Invalid config table\n");
			exit(1);
		}
		const char * key = lua_tostring(L,-2);	// 获取key
		if (lua_type(L,-1) == LUA_TBOOLEAN) {
			int b = lua_toboolean(L,-1);
			skynet_setenv(key,b ? "true" : "false" );
		} else {
			const char * value = lua_tostring(L,-1);	// 获取value
			if (value == NULL) {
				fprintf(stderr, "Invalid config table key = %s\n", key);
				exit(1);
			}
			skynet_setenv(key,value);
		}
		lua_pop(L,1);	// 弹出value,保留key,以便下一次迭代
	}
	lua_pop(L,1);		// 弹出全局变量表
}

int sigign() {
	struct sigaction sa;
	sa.sa_handler = SIG_IGN; // 忽略信号
	sigaction(SIGPIPE, &sa, 0);
	return 0;
}

int
main(int argc, char *argv[]) {
	const char * config_file = "config";
	if (argc > 1) {
		config_file = argv[1];
	}
	skynet_env_init();

	sigign();

	struct skynet_config config;

	// lua 相关的一些初始化
	struct lua_State *L = luaL_newstate();
	luaL_openlibs(L);	// link lua lib
	lua_close(L);

	L = luaL_newstate();

	int err = luaL_dofile(L, config_file);
	if (err) {
		fprintf(stderr,"%s\n",lua_tostring(L,-1));
		lua_close(L);
		return 1;
	} 

	// 初始化 lua 环境
	_init_env(L);

	const char *path = optstring("lua_path","./lualib/?.lua;./lualib/?/init.lua");
	setenv("LUA_PATH",path,1);

	const char *cpath = optstring("lua_cpath","./luaclib/?.so");
	setenv("LUA_CPATH",cpath,1);
	optstring("luaservice","./service/?.lua");

	// 加载配置项
	config.thread =  optint("thread",8);
	config.module_path = optstring("cpath","./service/?.so");
	config.logger = optstring("logger",NULL);
	config.harbor = optint("harbor", 1);
	config.master = optstring("master","127.0.0.1:2012");
	config.start = optstring("start","main.lua");
	config.local = optstring("address","127.0.0.1:2525");
	config.standalone = optstring("standalone",NULL);

	lua_close(L);

	skynet_start(&config);

	printf("skynet exit\n");

	return 0;
}
