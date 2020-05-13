#include <cstdlib>
#include <istream>
#include <thread>
#include <functional>

extern "C" {
#include "lua5.3/lua.h"
#include "lua5.3/lualib.h"
#include "lua5.3/lauxlib.h"
#include "lua_utf8.c"
}

static void *lua_allocator(void *data, void *ptr, size_t old_size, size_t new_size) {
    auto *computer = static_cast<Computer *>(data);
    if (new_size) {
        if (ptr) { // reallocate
            if (computer->used_memory + new_size - old_size > computer->memory) {
                fprintf(stderr, "lua_allocator: refusing to reallocate %zu new bytes for %s\n", new_size - old_size, computer->name.c_str());
                return NULL;
            }
            computer->used_memory -= (long long) old_size;
            computer->used_memory += (long long) new_size;
            return realloc(ptr, new_size);
        } else { // allocate
            if (computer->used_memory + new_size > computer->memory) {
                fprintf(stderr, "lua_allocator: refusing to allocate %zu new bytes for %s\n", new_size, computer->name.c_str());
                return NULL;
            }
            computer->used_memory += (long long) new_size;
            return malloc(new_size);
        }
    } else { // free
        computer->used_memory -= (long long) old_size;
        free(ptr);
        return NULL;
    }
}

static const char *lua_stream_reader(lua_State *state, void *data, size_t *size) {
    static const int MAX_BYTES = 1024;
    auto *stream = static_cast<std::istream *>(data);
    auto *bytes = new char[MAX_BYTES + 1];
    int count = stream->readsome(bytes, MAX_BYTES);
    *size = count;
    bytes[count] = '\0';
    return bytes;
}

static Computer *get_computer_upvalue(lua_State *state, int i) {
    return static_cast<Computer *>(lua_touserdata(state, lua_upvalueindex(i)));
}

static int api_table_stub(lua_State *state) {
    if (lua_isstring(state, 2)) {
        string s1 = lua_tostring(state, lua_upvalueindex(2));
        string s2 = lua_tostring(state, 2);
        printf("warning: %s: key '%s' not found\n", s1.c_str(), s2.c_str());
    }
    lua_pushnil(state);
    int value = lua_gettop(state);
    lua_createtable(state, 0, 0);
    int metatable = lua_gettop(state);
    lua_pushliteral(state, "__call");
    lua_pushliteral(state, "");
    lua_copy(state, lua_upvalueindex(2), lua_gettop(state));
    lua_pushliteral(state, "");
    lua_copy(state, 2, lua_gettop(state));
    lua_pushcclosure(state, [](lua_State *state) -> int {
        string s1 = lua_tostring(state, lua_upvalueindex(1));
        string s2 = lua_tostring(state, lua_upvalueindex(2));
        fprintf(stderr, "error: %s: invoking nonexistent api function '%s'\n", s1.c_str(), s2.c_str());
        lua_pushliteral(state, "attempt to call a nil value");
        lua_error(state);
        return 0;
    }, 2);
    lua_settable(state, metatable);
    lua_setmetatable(state, value);
    return 1;
}

class ComponentAPI {
private:
    ComponentAPI() = default;

public:
    static int type(lua_State *state) {
        string address = lua_tostring(state, 1);
        auto *computer = get_computer_upvalue(state, 1);
        Component *component = computer->get_component(address);
        if (component) {
            lua_pushstring(state, component->get_type().c_str());
            return 1;
        } else api_error(state, ("type: no such component: " + address).c_str());
    }

    static int list(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 2);

        int argc = lua_gettop(state);
        string filter;
        bool exact = false;
        if (argc == 0) {
            filter = "";
        } else {
            if (lua_isnil(state, 1)) filter = "";
            else {
                const char *cFilter = lua_tostring(state, 1);
                if (cFilter) filter = cFilter;
                else {
                    api_error(state, "invalid argument #1");
                }
                if (argc > 1) {
                    exact = lua_toboolean(state, 2);
                }
            }
        }
        lua_pop(state, argc);

        std::vector<Component *> components;
        computer->get_components(&components);

        lua_createtable(state, 0, components.size());
        int table = lua_gettop(state);
        for (Component *component : components) {
            string type = component->get_type();
            bool match;
            if (filter.length() > 0) {
                if (exact) match = type == filter;
                else match = type.find(filter) != string::npos;
            } else {
                match = true;
            }
            if (!match) continue;
            lua_pushstring(state, component->address.c_str());
            lua_pushstring(state, type.c_str());
            lua_settable(state, table);
        }

        lua_createtable(state, 0, 1);
        int metatable = lua_gettop(state);
        lua_pushliteral(state, "__call");
        lua_pushliteral(state, "");
        lua_copy(state, lua_upvalueindex(1), lua_gettop(state));
        lua_pushliteral(state, "");
        lua_copy(state, table, lua_gettop(state));
        lua_createtable(state, 0, 1);
        lua_pushcclosure(state, [](lua_State *state) -> int {
            int key_table = lua_upvalueindex(3);
            lua_pushliteral(state, "");
            lua_copy(state, lua_upvalueindex(1), lua_gettop(state));
            lua_pushliteral(state, "");
            lua_copy(state, lua_upvalueindex(2), lua_gettop(state));
            lua_pushliteral(state, "key");
            lua_gettable(state, key_table);
            lua_call(state, 2, 2);
            lua_pushliteral(state, "key");
            lua_pushliteral(state, "");
            lua_copy(state, lua_gettop(state) - 3, lua_gettop(state));
            lua_settable(state, key_table);
            return 2;
        }, 3);
        lua_settable(state, metatable);

        lua_setmetatable(state, table);

        return 1;
    }

    static int invoke(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        lua_rotate(state, 1, -2);
        string address = lua_tostring(state, lua_gettop(state) - 1);
        string method = lua_tostring(state, lua_gettop(state));
        lua_pop(state, 2);
        Component *component = computer->get_component(address);
        if (component) {
            return component->invoke(method, state);
        } else {
            string error = "invoke: no such component: ";
            error += address;
            lua_pushstring(state, error.c_str());
            lua_error(state);
            return 0;
        }
    }

    static int proxy(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        string address = lua_tostring(state, 1);
        Component *component = computer->get_component(address);
        if (!component) api_error(state, ("proxy: no such component: " + address).c_str());
        auto methods = component->get_methods();
        lua_createtable(state, 0, methods.size());
        int table = lua_gettop(state);
        for (auto[name, direct] : methods) {
            lua_pushstring(state, name.c_str());
            lua_pushlightuserdata(state, component);
            lua_pushstring(state, name.c_str());
            lua_pushcclosure(state, [](lua_State *state) -> int {
                auto *component = static_cast<Component *>(lua_touserdata(state, lua_upvalueindex(1)));
                string method = lua_tostring(state, lua_upvalueindex(2));
                return component->invoke(method, state);
            }, 2);
            lua_settable(state, table);
        }
        lua_pushliteral(state, "address");
        lua_pushstring(state, component->address.c_str());
        lua_settable(state, table);
        lua_pushliteral(state, "type");
        lua_pushstring(state, component->get_type().c_str());
        lua_settable(state, table);
        lua_createtable(state, 0, 1);
        int metatable = lua_gettop(state);
        lua_pushliteral(state, "__index");
        lua_pushliteral(state, "");
        lua_copy(state, table, lua_gettop(state));
        lua_pushfstring(state, "proxy for component %s", component->name.c_str());
        lua_pushcclosure(state, api_table_stub, 2);
        lua_settable(state, metatable);
        lua_setmetatable(state, table);
        return 1;
    }
};

static bool signal_yield = false;
static long long signal_deadline = 0;

class ComputerAPI {
private:
    ComputerAPI() = default;

public:
    static int set_architecture(lua_State *state) {
        return 0;
    }

    static int address(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        lua_pushstring(state, computer->address.c_str());
        return 1;
    }

    static int uptime(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        long long uptime = (now - computer->start_time) / 1000;
        lua_pushnumber(state, uptime);
        return 1;
    }

    static int tmp_address(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        lua_pushstring(state, computer->tmp_fs->address.c_str());
        return 1;
    }

    static int free_memory(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        lua_pushinteger(state, computer->memory - computer->used_memory);
        return 1;
    }

    static int total_memory(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        lua_pushinteger(state, computer->memory);
        return 1;
    }

    static int push_signal(lua_State *state) {
        auto *computer = get_computer_upvalue(state, 1);
        if (lua_gettop(state) < 1) api_error(state, "computer.pushSignal(): at least one argument expected");
        int n = lua_gettop(state);
        lua_pushliteral(state, "");
        for (int i = 1; i <= n; i++) {
            lua_pushliteral(state, ", ");
            lua_pushliteral(state, "");
            lua_copy(state, lua_upvalueindex(2), lua_gettop(state));
            lua_pushliteral(state, "");
            lua_copy(state, i, lua_gettop(state));
            lua_call(state, 1, 1);
            lua_concat(state, 3);
        }
        for (int i = 1; i <= n; i++) {
            lua_remove(state, 1);
        }
        string s = lua_tostring(state, lua_gettop(state));
        lua_pop(state, 1);
        s = s.substr(2);
        computer->signal_queue.push(s);
        //printf("push_signal: %s\n", s.c_str());
        return 0;
    }

    static int pull_signal_k(lua_State *state, int status, lua_KContext ctx) {
        auto *computer = get_computer_upvalue(state, 1);
        if (computer->signal_queue.empty()) {
            if (signal_deadline != 0 && get_current_time() > signal_deadline) {
                //printf("pull_signal: timeout\n");
                signal_yield = false;
                return 0;
            }
            return pull_signal_k(state, lua_yieldk(state, 0, 0, pull_signal_k), 0);
        } else {
            string signal = computer->signal_queue.front();
            //printf("pull_signal: %s\n", signal.c_str());
            computer->signal_queue.pop();
            signal = "return " + signal;
            std::istringstream signal_stream(signal);
            lua_pop(state, lua_gettop(state));
            lua_load(state, lua_stream_reader, &signal_stream, "signalLoad", "t");
            lua_call(state, 0, LUA_MULTRET);
            signal_yield = false;
            return lua_gettop(state);
        }
    }

    static int pull_signal(lua_State *state) {
        if (lua_gettop(state) > 1) api_error(state, "pullSignal: invalid number of arguments");
        if (lua_gettop(state) == 1) {
            if (!lua_isnumber(state, 1)) api_error(state, "pullSignal: invalid type of argument #1");
            double seconds = lua_tonumber(state, 1);
            if (seconds != (1. / 0.)) signal_deadline = get_current_time() + (long long) (seconds * 1000);
            else signal_deadline = 0;
        } else {
            signal_deadline = 0;
        }
        signal_yield = true;
        return pull_signal_k(state, LUA_OK, 0);
    }

    static int shutdown(lua_State *state) {
        bool reboot = lua_toboolean(state, 1);
        luaL_traceback(state, state, "Computer shut down.", 1);
        string traceback = lua_tostring(state, lua_gettop(state));
        printf("%s\n", traceback.c_str());
        if(reboot) fprintf(stderr, "Rebooting isn't supported, please restart computer manually!");
        auto *quit = new SDL_Event;
        quit->type = SDL_QUIT;
        SDL_PushEvent(quit);
        return 0;
    }

    static int beep(lua_State *state) {
        // TODO: add something
        return 0;
    }

    static int get_program_locations(lua_State *state) {
        // nothing to do here
        lua_createtable(state, 0, 1);
        lua_pushliteral(state, "n");
        lua_pushinteger(state, 0);
        lua_settable(state, lua_gettop(state) - 2);
        return 1;
    }
};

class UnicodeAPI {
private:
    UnicodeAPI() = default;

public:
    static int sub(lua_State *state) {
        return Lutf8_sub(state);
    }

    static int len(lua_State *state) {
        return Lutf8_len(state);
    }

    static int chr(lua_State *state) {
        return Lutf8_char(state);
    }

    static int wlen(lua_State *state) {
        return Lutf8_width(state);
    }

    static int wtrunc(lua_State *state) {
        string s = lua_tostring(state, 1);
        int c = lua_tointeger(state, 2);
        lua_pop(state, 1);
        Lutf8_width(state);
        int w = lua_tointeger(state, lua_gettop(state));
        lua_pop(state, lua_gettop(state));
        if (w < c - 1) api_error(state, "not enough characters");
        for (int j = w; j >= 1; j--) {
            lua_pop(state, lua_gettop(state));
            lua_pushstring(state, s.c_str());
            lua_pushinteger(state, 1);
            lua_pushinteger(state, j);
            Lutf8_sub(state);
            while(lua_gettop(state) > 1) lua_remove(state, 1);
            Lutf8_width(state);
            int cw = lua_tointeger(state, lua_gettop(state));
            lua_pop(state, lua_gettop(state));
            if (cw < c) {
                lua_pushstring(state, s.c_str());
                lua_pushinteger(state, 1);
                lua_pushinteger(state, j);
                Lutf8_sub(state);
                return 1;
            }
        }
        lua_pushliteral(state, "");
        return 1;
    }

    static int char_width(lua_State *state) {
        if(lua_gettop(state) < 1) api_error(state, "unicode.charWidth: invalid number of arguments");
        if(lua_gettop(state) > 1) lua_pop(state, lua_gettop(state) - 1);
        if(!lua_isstring(state, lua_gettop(state))) api_error(state, "unicode.charWidth: invalid type of argument #1");
        auto *s = lua_tostring(state, 1);
        auto *e = s + strlen(s);
        auto *n = utf8_next(s, e);
        lua_pushinteger(state, n - s);
        return 1;
    }

    static int is_wide(lua_State *state) {
        char_width(state);
        lua_pushboolean(state, lua_tointeger(state, lua_gettop(state)) > 1);
        return 1;
    }

    static int lower(lua_State *state) {
        return Lutf8_lower(state);
    }

    static int upper(lua_State *state) {
        return Lutf8_upper(state);
    }

    static int reverse(lua_State *state) {
        return Lutf8_reverse(state);
    }
};

static void create_environment(Computer *computer, lua_State *state) {
    // adding standard libs
    luaL_openlibs(state);

    // component library
    {
        lua_createtable(state, 0, 0);
        int component_table = lua_gettop(state);

        lua_pushliteral(state, "type");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComponentAPI::type, 1);
        lua_settable(state, component_table);

        lua_pushliteral(state, "list");
        lua_getglobal(state, "next");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComponentAPI::list, 2);
        lua_settable(state, component_table);

        lua_pushliteral(state, "invoke");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComponentAPI::invoke, 1);
        lua_settable(state, component_table);

        lua_pushliteral(state, "proxy");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComponentAPI::proxy, 1);
        lua_settable(state, component_table);

        lua_createtable(state, 0, 1);
        int component_metatable = lua_gettop(state);
        lua_pushliteral(state, "__index");
        lua_pushliteral(state, "");
        lua_copy(state, component_table, lua_gettop(state));
        lua_pushliteral(state, "component");
        lua_pushcclosure(state, api_table_stub, 2);
        lua_settable(state, component_metatable);
        lua_setmetatable(state, component_table);

        lua_setglobal(state, "component");
    }


    // computer library
    {
        lua_createtable(state, 0, 0);
        int computer_table = lua_gettop(state);

        lua_pushliteral(state, "setArchitecture");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::set_architecture, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "address");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::address, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "uptime");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::uptime, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "tmpAddress");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::tmp_address, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "freeMemory");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::free_memory, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "totalMemory");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::total_memory, 1);
        lua_settable(state, computer_table);

        string serialize = "local function serialize(o)\n"
                           "        if type(o) == \"nil\" then\n"
                           "                return \"nil\"\n"
                           "        elseif type(o) == \"boolean\" then\n"
                           "                return tostring(o)\n"
                           "        elseif type(o) == \"number\" then\n"
                           "                return tostring(o)\n"
                           "        elseif type(o) == \"string\" then\n"
                           "                local ret = \"\"\n"
                           "                for i=1,#o do\n"
                           "                        local char = string.sub(o, i, i)\n"
                           "                        if char == \"\\\"\" then\n"
                           "                                ret = ret .. \"\\\\\\\"\"\n"
                           "                        elseif char == \"\\n\" then\n"
                           "                                ret = ret .. \"\\\\n\"\n"
                           "                        elseif char == \"\\\\\" then\n"
                           "                                ret = ret .. \"\\\\\\\\\"\n"
                           "                        elseif char == \"\\t\" then \n"
                           "                                ret = ret .. \"\\\\t\"\n"
                           "                        else \n"
                           "                                ret = ret .. char\n"
                           "                        end\n"
                           "                end\n"
                           "                return \"\\\"\" .. ret .. \"\\\"\"\n"
                           "        elseif type(o) == \"table\" then\n"
                           "                local ret = \"{\"\n"
                           "                for k, v in pairs(o) do\n"
                           "                        ret = ret .. \"[\" .. serialize(k) .. \"]=\" .. serialize(v) .. \", \"\n"
                           "                end\n"
                           "                ret = ret .. \"}\"\n"
                           "                return ret\n"
                           "        else\n"
                           "                error(\"unsupported type: \" .. type(o))\n"
                           "        end\n"
                           "end\n"
                           "return serialize";

        lua_pushliteral(state, "pushSignal");
        lua_pushlightuserdata(state, computer);
        std::istringstream serializeStream(serialize);
        lua_load(state, lua_stream_reader, &serializeStream, "serializeLoad", "t");
        lua_call(state, 0, 1);
        lua_pushcclosure(state, ComputerAPI::push_signal, 2);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "pullSignal");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::pull_signal, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "shutdown");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::shutdown, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "beep");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::beep, 1);
        lua_settable(state, computer_table);

        lua_pushliteral(state, "getProgramLocations");
        lua_pushlightuserdata(state, computer);
        lua_pushcclosure(state, ComputerAPI::get_program_locations, 1);
        lua_settable(state, computer_table);

        lua_createtable(state, 0, 1);
        int computer_metatable = lua_gettop(state);
        lua_pushliteral(state, "__index");
        lua_pushliteral(state, "");
        lua_copy(state, computer_table, lua_gettop(state));
        lua_pushliteral(state, "computer");
        lua_pushcclosure(state, api_table_stub, 2);
        lua_settable(state, computer_metatable);
        lua_setmetatable(state, computer_table);

        lua_setglobal(state, "computer");
    }

    //checkArg function
    string checkArg = "local function checkArg(n, have, ...)\n"
                      "  have = type(have)\n"
                      "  local function check(want, ...)\n"
                      "    if not want then\n"
                      "      return false\n"
                      "    else\n"
                      "      return have == want or check(...)\n"
                      "    end\n"
                      "  end\n"
                      "  if not check(...) then\n"
                      "    local msg = string.format(\"bad argument #%d (%s expected, got %s)\",\n"
                      "                              n, table.concat({...}, \" or \"), have)\n"
                      "    error(msg, 3)\n"
                      "  end\n"
                      "end\n"
                      "return checkArg";
    std::istringstream checkArgStream(checkArg);
    lua_load(state, lua_stream_reader, &checkArgStream, "checkArgLoad", "t");
    lua_call(state, 0, 1);
    lua_setglobal(state, "checkArg");


    // unicode library (complete)
    {
        lua_createtable(state, 0, 0);
        int unicode_table = lua_gettop(state);

        lua_pushliteral(state, "sub");
        lua_pushcclosure(state, UnicodeAPI::sub, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "len");
        lua_pushcclosure(state, UnicodeAPI::len, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "char");
        lua_pushcclosure(state, UnicodeAPI::chr, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "wlen");
        lua_pushcclosure(state, UnicodeAPI::wlen, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "wtrunc");
        lua_pushcclosure(state, UnicodeAPI::wtrunc, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "charWidth");
        lua_pushcclosure(state, UnicodeAPI::char_width, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "isWide");
        lua_pushcclosure(state, UnicodeAPI::is_wide, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "lower");
        lua_pushcclosure(state, UnicodeAPI::lower, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "upper");
        lua_pushcclosure(state, UnicodeAPI::upper, 0);
        lua_settable(state, unicode_table);

        lua_pushliteral(state, "reverse");
        lua_pushcclosure(state, UnicodeAPI::reverse, 0);
        lua_settable(state, unicode_table);

        lua_createtable(state, 0, 1);
        int unicode_metatable = lua_gettop(state);
        lua_pushliteral(state, "__index");
        lua_pushliteral(state, "");
        lua_copy(state, unicode_table, lua_gettop(state));
        lua_pushliteral(state, "unicode");
        lua_pushcclosure(state, api_table_stub, 2);
        lua_settable(state, unicode_metatable);
        lua_setmetatable(state, unicode_table);

        lua_setglobal(state, "unicode");
    }

    lua_getglobal(state, "print");
    lua_setglobal(state, "rprint"); // for debugging


    // removing some standart libs
    lua_pushnil(state);
    lua_setglobal(state, "require");

    string os_allowed = "local allowed = {}; ";
    os_allowed += "allowed.time = true; allowed.clock = true; allowed.date = true; allowed.difftime = true;";
    string remove_os_libs = os_allowed + " for k, v in pairs(os) do if not allowed[k] then os[k] = nil end end";
    std::istringstream remove_os_libs_stream(remove_os_libs);
    lua_load(state, lua_stream_reader, &remove_os_libs_stream, "removeOSLibs", "t");
    lua_call(state, 0, 0);
}

static void emulate_computer(Computer *computer, std::istream *stream) {
    lua_State *state = lua_newstate(lua_allocator, computer);
    lua_State *boot = lua_newstate(lua_allocator, computer);
    lua_load(boot, lua_stream_reader, stream, "boot", "t");
    create_environment(computer, boot);
    int status;
    if (lua_isstring(boot, lua_gettop(boot))) {
        std::cerr << "wtf\n";
        std::cerr << lua_tostring(boot, lua_gettop(boot));
    }
    while ((status = lua_resume(boot, state, 0)) == LUA_YIELD) {
        luaL_traceback(state, boot, NULL, 1);
        string traceback = lua_tostring(state, lua_gettop(state));
        //printf("yield: %s\n", traceback.c_str());
        if (signal_yield) {
            std::unique_lock<std::mutex> locker(computer->queue_lock);
            long long time = signal_deadline - get_current_time();
            if (signal_deadline != 0) computer->queue_notifier.wait_for(locker, std::chrono::milliseconds(time));
            else computer->queue_notifier.wait(locker);
            //printf("continue\n");
        }
    }
    if (status == LUA_OK) {
        std::cerr << "Computer halted\n";
    } else {
        std::cerr << "Computer crashed, status code " << status << "\n";
        string error;
        if (lua_isstring(boot, lua_gettop(boot))) {
            error = string(lua_tostring(boot, lua_gettop(boot)));
        } else error = "Error object cannot be converted to string";
        luaL_traceback(state, boot, error.c_str(), 1);
        std::cerr << lua_tostring(state, lua_gettop(state));
    }
}

static std::thread *boot_computer(Computer *computer) {
    std::vector<Component *> components;
    computer->get_components(&components);
    Eeprom *eeprom = nullptr;
    for (Component *component : components) {
        if (component->get_type() == EEPROM) eeprom = dynamic_cast<Eeprom *>(component);
    }
    auto stream = new std::istringstream(eeprom->get_primary());
    auto *thread = new std::thread(emulate_computer, computer, stream);
    return thread;
}
