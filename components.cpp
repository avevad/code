#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-emplace"

#include "components.h"
#include "computer.h"
#include <string>
#include <fstream>
#include <chrono>
#include "lua_utf8.c"
#include <lua5.3/lua.h>
#include <SDL2/SDL_ttf.h>
#include "curl/curl.h"

#define api_error(L, s) {std::cerr << "api error: " << s << std::endl; lua_pushstring(L, s); lua_error(L); return 0;}

using std::string;

string get_component_folder(const string &project_dir, const string &component_type, const string &component_name) {
    return project_dir + COMPONENTS_FOLDER + component_name + COMPONENT_TYPE_DELIMITER + component_type;
}

string get_component_address(const string &project_dir, const string &component_type, const string &component_name) {
    std::ifstream in(get_component_folder(project_dir, component_type, component_name) + COMPONENT_ADDRESS_FILE);
    string address;
    in >> address;
    return address;
}

Component::Component(string name, string address) : address(std::move(address)), name(std::move(name)) {

}

int Component::load_components(const string &project_dir, std::map<string, Component *> &components) {
    int cnt = 0;
    for (const auto &entry : std::filesystem::directory_iterator(project_dir + COMPONENTS_FOLDER)) {
        string path = entry.path();
        string folder_name = path.substr(path.find_last_of("/\\") + 1);
        size_t dot_index = folder_name.find_last_of('.');
        string name = folder_name.substr(0, dot_index);
        string type = folder_name.substr(dot_index + 1);
        Component *component = nullptr;
        if (type == EEPROM) {
            component = new Eeprom(project_dir, name);
        } else if (type == FILESYSTEM) {
            component = new Filesystem(project_dir, name);
        } else if (type == SCREEN) {
            component = new Screen(project_dir, name);
        } else if (type == GPU) {
            component = new Gpu(project_dir, name);
        } else if (type == KEYBOARD) {
            component = new Keyboard(project_dir, name);
        } else if (type == INTERNET) {
            component = new Internet(project_dir, name);
        }

        if (component) {
            components[name] = component;
            cnt++;
        }
    }
    return cnt;
}

Component::~Component() = default;


Eeprom::Eeprom(const string &project_dir,
               const string &name) : Component(name, get_component_address(project_dir, EEPROM, name)),
                                     project_dir(project_dir) {

}

string Eeprom::get_primary() {
    const string primary_data_file = get_component_folder(project_dir, EEPROM, name) + EEPROM_PRIMARY_DATA_FILE;
    if (std::filesystem::is_regular_file(primary_data_file)) {
        std::ifstream in(primary_data_file);
        char *buffer = new char[EEPROM_MAX_PRIMARY_SIZE + 1];
        size_t read = 0;
        while (read < EEPROM_MAX_PRIMARY_SIZE) {
            size_t new_read = in.readsome(buffer + read, EEPROM_MAX_PRIMARY_SIZE - read);
            if (new_read == 0) break;
            read += new_read;
        }
        buffer[read] = '\0';
        string ret = string(buffer);
        delete[] buffer;
        return ret;
    } else return "";
}



string Eeprom::get_secondary() {
    const string secondary_data_file = get_component_folder(project_dir, EEPROM, name) + EEPROM_SECONDARY_DATA_FILE;
    if (std::filesystem::is_regular_file(secondary_data_file)) {
        std::ifstream in(secondary_data_file);
        char *buffer = new char[EEPROM_MAX_SECONDARY_SIZE + 1];
        size_t read = 0;
        while (read < EEPROM_MAX_SECONDARY_SIZE) {
            size_t new_read = in.readsome(buffer + read, EEPROM_MAX_SECONDARY_SIZE - read);
            if (new_read == 0) break;
            read += new_read;
        }
        buffer[read] = '\0';
        string ret = string(buffer);
        delete[] buffer;
        return ret;
    } else return "";
}

int Eeprom::invoke(const string &method, lua_State *state) {
    if (method == "getSize") {
        lua_pushinteger(state, EEPROM_MAX_PRIMARY_SIZE);
        return 1;
    } else if (method == "getDataSize") {
        lua_pushinteger(state, EEPROM_MAX_SECONDARY_SIZE);
        return 1;
    } else if (method == "get") {
        lua_pushstring(state, get_primary().c_str());
        return 1;
    } else if (method == "getData") {
        lua_pushstring(state, get_secondary().c_str());
        return 1;
    } else if (method == "getLabel") {
        std::ifstream in(get_component_folder(project_dir, EEPROM, name) + EEPROM_LABEL_FILE);
        string label;
        std::getline(in, label);
        lua_pushstring(state, label.c_str());
        return 1;
    } else {
        string error = EEPROM + ": no such method: ";
        error += method;
        std::cerr << error << std::endl;
        lua_pushstring(state, error.c_str());
        lua_error(state);
        return 0;
    }
}

std::vector<std::pair<string, bool>> Eeprom::get_methods() {
    std::vector<std::pair<string, bool>> result;
    result.push_back({"getSize", 1});
    result.push_back({"get", 1});
    result.push_back({"getData", 1});
    result.push_back({"getLabel", 1});
    result.push_back({"getDataSize", 1});
    return result;
}

string Eeprom::get_type() {
    return EEPROM;
}

Eeprom::~Eeprom() = default;


Filesystem::Filesystem(const string &project_dir,
                       const string &name) : Component(name, get_component_address(project_dir, FILESYSTEM, name)),
                                             project_dir(project_dir) {

}

int Filesystem::invoke(const string &method, lua_State *state) {
    if (method == "isDirectory") {
        if (lua_gettop(state) != 1) api_error(state, "isDirectory(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        if (cPath) {
            bool is_directory = std::filesystem::is_directory(get_data_directory() + string(cPath));
            lua_pushboolean(state, is_directory);
            return 1;
        } else api_error(state, "isDirectory(): invalid type of argument #1");
    } else if (method == "makeDirectory") {
        if (lua_gettop(state) != 1) api_error(state, "makeDirectory(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        if (cPath) {
            bool ok = std::filesystem::create_directories(get_data_directory() + string(cPath));
            lua_pushboolean(state, ok);
            return 1;
        } else api_error(state, "makeDirectory(): invalid type of argument #1");
    } else if (method == "exists") {
        if (lua_gettop(state) != 1) api_error(state, "exists(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        if (cPath) {
            string path = get_data_directory() + string(cPath);
            bool exists = std::filesystem::exists(path);
            lua_pushboolean(state, exists);
            return 1;
        } else api_error(state, "exists(): invalid type of argument #1");
    } else if (method == "size") {
        if (lua_gettop(state) != 1) api_error(state, "size(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        if (cPath) {
            string path = get_data_directory() + string(cPath);
            std::error_code err;
            long long size = std::filesystem::file_size(path, err);
            if (err) {
                lua_pushinteger(state, 0);
                return 1;
            } else {
                lua_pushinteger(state, size);
                return 1;
            }
        } else api_error(state, "size(): invalid type of argument #1");
    } else if (method == "lastModified") {
        if (lua_gettop(state) != 1) api_error(state, "size(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        if (cPath) {
            string path = get_data_directory() + string(cPath);
            std::error_code err;
            auto time = std::filesystem::last_write_time(path, err);
            using std::chrono_literals::operator""s;
            time += 6437664000s; // something about 204 years, idk why I should do it
            if (err.value()) {
                lua_pushinteger(state, 0);
                return 1;
            } else {
                lua_pushinteger(state,
                                std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count());
                return 1;
            }
        } else api_error(state, "lastModified(): invalid type of argument #1");
    } else if (method == "remove") {
        if (lua_gettop(state) != 1) api_error(state, "remove(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        if (cPath) {
            string path = get_data_directory() + string(cPath);
            int ok = std::filesystem::remove_all(path);
            lua_pushboolean(state, ok > 0);
            return 1;
        } else api_error(state, "remove(): invalid type of argument #1");
    } else if (method == "rename") {
        if (lua_gettop(state) != 2) api_error(state, "rename(): invalid number of arguments");
        auto *cPathSrc = lua_tostring(state, 1);
        auto *cPathDst = lua_tostring(state, 2);
        if (cPathSrc && cPathDst) {
            string pathSrc = get_data_directory() + string(cPathSrc);
            string pathDst = get_data_directory() + string(cPathDst);
            std::error_code err;
            std::filesystem::rename(pathSrc, pathDst, err);
            lua_pushboolean(state, err.value() == 0);
            return 1;
        } else api_error(state, "rename(): invalid type of argument");
    } else if (method == "open") {
        if (lua_gettop(state) < 1 || lua_gettop(state) > 2) api_error(state, "open(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        string mode = "r";
        if (lua_gettop(state) == 2) {
            auto *cMode = lua_tostring(state, 2);
            if (cMode) mode = cMode;
            else api_error(state, "open(): invalid type of argument #2");
        }
        if (cPath) {
            string path = get_data_directory() + string(cPath);
            auto fMode = std::ios_base::in;
            if (mode == "r" || mode == "rb") {
                fMode = std::ios_base::in;
            } else if (mode == "a" || mode == "ab") {
                fMode = std::ios_base::out | std::ios_base::ate;
            } else if (mode == "w" || mode == "wb") {
                fMode = std::ios_base::out;
            } else {
                api_error(state, ("open(): unknown mode" + mode).c_str());
            }
            auto *stream = new std::fstream(path, fMode);
            auto *descriptor = new Descriptor(stream);
            int descriptor_id = descriptors.size();
            if (free_descriptors.empty()) descriptors.push_back(descriptor);
            else {
                descriptor_id = free_descriptors.front();
                free_descriptors.pop();
                descriptors[descriptor_id] = descriptor;
            }
            //printf("filesystem.open(): new descriptor #%d, path '%s', mode '%s'\n", descriptor_id, cPath, mode.c_str());
            lua_pushinteger(state, descriptor_id);
            return 1;
        } else api_error(state, "open(): invalid type of argument #1");
    } else if (method == "read") {
        if (lua_gettop(state) != 2) api_error(state, "read(): invalid number of arguments");
        int ok = 0;
        int handle = lua_tointegerx(state, 1, &ok);
        if (ok == 0) api_error(state, "read(): invalid type of argument #1");
        double dCount = lua_tonumberx(state, 2, &ok);
        if (ok == 0) api_error(state, "read(): invalid type of argument #2");
        int count = dCount;
        if (count <= 0) count = INT32_MAX;
        if (handle < 0 || handle >= descriptors.size()) api_error(state, "read(): no such descriptor");
        Descriptor *descriptor = descriptors[handle];
        if (!descriptor) api_error(state, "read(): no such descriptor");
        if (descriptor->stream->eof())
            return 0;
        count = std::min(FILESYSTEM_MAX_BUFFER_SIZE, count);
        char *buffer = new char[count + 1];
        descriptor->stream->read(buffer, count);
        int read = descriptor->stream->gcount();
        //printf("filesystem.write(): read %d bytes from #%d\n", read, handle);
        string real(buffer, read);
        lua_pushstring(state, real.c_str());
        return 1;
    } else if (method == "write") {
        if (lua_gettop(state) != 2) api_error(state, "write(): invalid number of arguments");
        int ok = 0;
        int handle = lua_tointegerx(state, 1, &ok);
        if (ok == 0) api_error(state, "write(): invalid type of argument #1");
        auto *cS = lua_tostring(state, 2);
        if (!cS) api_error(state, "write(): invalid type of argument #2");
        if (handle < 0 || handle >= descriptors.size()) api_error(state, "read(): no such descriptor");
        Descriptor *descriptor = descriptors[handle];
        if (!descriptor) api_error(state, "write(): no such descriptor");
        int n = strlen(cS);
        descriptor->stream->write(cS, n);
        //printf("filesystem.write(): wrote %d bytes to #%d\n", n, handle);
        lua_pushboolean(state, !descriptor->stream->bad());
        return 1;
    } else if (method == "seek") {
        if (lua_gettop(state) != 3) api_error(state, "seek(): invalid number of arguments");
        int ok = 0;
        int handle = lua_tointegerx(state, 1, &ok);
        if (ok == 0) api_error(state, "seek(): invalid type of argument #1");
        auto *cWhence = lua_tostring(state, 2);
        if(!cWhence) api_error(state, "seek(): invalid type of argument #2");
        string whence = cWhence;
        if(!lua_isnumber(state, 3)) api_error(state, "seek(): invalid type of argument #3");
        int off = lua_tonumber(state, 3);
        if (handle < 0 || handle >= descriptors.size()) api_error(state, "seek(): no such descriptor");
        Descriptor *descriptor = descriptors[handle];
        if (!descriptor) api_error(state, "seek(): no such descriptor");
        int cur = descriptor->stream->tellg();
        auto pos = std::fstream::cur;
        if(whence == "cur") {
            pos = std::fstream::cur;
            if(-off > pos) off = -pos;
        } else if(whence == "set") {
            pos = std::fstream::beg;
            if(off < 0) off = 0;
        } else if(whence == "end") {
            pos = std::fstream::end;
            if(off > 0) off = 0;
        } else api_error(state, "seek(): invalid argument #2");
        descriptor->stream->seekg(off, pos);
        lua_pushinteger(state, descriptor->stream->tellg());
        return 1;
    } else if (method == "close") {
        if (lua_gettop(state) != 1) api_error(state, "close(): invalid number of arguments");
        int ok = 0;
        int handle = lua_tointegerx(state, 1, &ok);
        if (ok == 0) api_error(state, "close(): invalid type of argument #1");
        if (handle < 0 || handle >= descriptors.size()) api_error(state, "close(): no such descriptor");
        Descriptor *descriptor = descriptors[handle];
        if (!descriptor) api_error(state, "close(): no such descriptor");
        descriptor->stream->flush();
        descriptor->stream->close();
        delete descriptor;
        descriptors[handle] = nullptr;
        free_descriptors.push(handle);
        //printf("filesystem.close(): closed #%d\n", handle);
        return 0;
    } else if (method == "list") {
        if (lua_gettop(state) != 1) api_error(state, "list(): invalid number of arguments");
        auto *cPath = lua_tostring(state, 1);
        if (cPath) {
            string rPath = cPath;
            if (!std::filesystem::is_directory(get_data_directory() + rPath))
                return 0;
            lua_createtable(state, 0, 0);
            int table = lua_gettop(state);
            int n = 0;
            for (const auto &entry : std::filesystem::directory_iterator(get_data_directory() + rPath)) {
                n++;
                string path = entry.path();
                string name = path.substr(path.find_last_of("/\\") + 1);
                if (entry.is_directory()) name += "/";
                lua_pushstring(state, name.c_str());
                lua_seti(state, table, n);
            }
            lua_pushliteral(state, "n");
            lua_pushinteger(state, n);
            lua_settable(state, table);
            return 1;
        } else api_error(state, "list(): invalid type of argument #1");
    } else if (method == "isReadOnly") {
        lua_pushboolean(state, is_readonly());
        return 1;
    } else if (method == "getLabel") {
        lua_pushstring(state, get_label().c_str());
        return 1;
    } else if (method == "setLabel") {
        string label = lua_tostring(state, 1);
        set_label(label);
        return 1;
    } else if (method == "spaceUsed") {
        lua_pushinteger(state, space_used());
        return 1;
    } else if (method == "spaceTotal") {
        lua_pushinteger(state, std::filesystem::space(get_data_directory()).free + space_used());
        return 1;
    } else {
        string error = FILESYSTEM + ": no such method: ";
        error += method;
        std::cerr << error << std::endl;
        lua_pushstring(state, error.c_str());
        lua_error(state);
        return 0;
    }
}

std::vector<std::pair<string, bool>> Filesystem::get_methods() {
    std::vector<std::pair<string, bool>> result;
    result.push_back({"isDirectory", true});
    result.push_back({"exists", true});
    result.push_back({"size", true});
    result.push_back({"lastModified", true});
    result.push_back({"remove", true});
    result.push_back({"rename", true});
    result.push_back({"open", true});
    result.push_back({"read", true});
    result.push_back({"write", true});
    result.push_back({"seek", true});
    result.push_back({"close", true});
    result.push_back({"list", true});
    result.push_back({"isReadOnly", true});
    result.push_back({"getLabel", true});
    result.push_back({"setLabel", true});
    result.push_back({"makeDirectory", true});
    result.push_back({"spaceUsed", true});
    result.push_back({"spaceTotal", true});
    return result;
}

string Filesystem::get_type() {
    return FILESYSTEM;
}

string Filesystem::get_data_directory() {
    return get_component_folder(project_dir, FILESYSTEM, name) + FILESYSTEM_DATA_FOLDER;
}

bool Filesystem::is_readonly() {
    return
            std::filesystem::is_regular_file(
                    get_component_folder(project_dir, FILESYSTEM, name) + FILESYSTEM_READONLY_MARKER
            );
}

string Filesystem::get_label() {
    std::ifstream in(get_component_folder(project_dir, FILESYSTEM, name));
    string label;
    std::getline(in, label);
    return label;
}

void Filesystem::set_label(const string &label) {
    std::ofstream out(get_component_folder(project_dir, FILESYSTEM, name));
    out << label;
}

unsigned long long Filesystem::space_used() {
    unsigned long long space = 0;
    for(const auto& entry : std::filesystem::recursive_directory_iterator(get_component_folder(project_dir, FILESYSTEM, name) + FILESYSTEM_DATA_FOLDER)) {
        if(entry.is_regular_file()) space += entry.file_size();
    }
    return space;
}

Filesystem::~Filesystem() {
    for (Descriptor *descriptor : descriptors) {
        delete descriptor;
    }
    delete &descriptors;
    delete &free_descriptors;
}

Filesystem::Descriptor::Descriptor(std::fstream *stream) : stream(stream) {

}

Filesystem::Descriptor::~Descriptor() {
    stream->close();
    delete stream;
}

Screen::Screen(const string &project_dir,
               const string &name) : Component(name, get_component_address(project_dir, SCREEN, name)),
                                     window(SDL_CreateWindow(name.c_str(), 0, 0, 100, 100, SDL_WINDOW_SHOWN)),
                                     surface(SDL_GetWindowSurface(window)),
                                     project_dir(project_dir) {
    std::ifstream in(get_component_folder(project_dir, SCREEN, name) + SCREEN_CONFIG_FILE);
    in >> color_depth >> ratio_width >> ratio_height >> max_width >> max_height;
    update_size(max_width, max_height);
    in = std::ifstream(get_component_folder(project_dir, SCREEN, name) + SCREEN_KEYBOARDS_FILE);
    string keyboard;
    while (in >> keyboard) {
        keyboards.push_back(keyboard);
    }
}

int Screen::invoke(const string &method, lua_State *state) {
    if (method == "getKeyboards") {
        lua_createtable(state, keyboards.size(), 1);
        int keyboards_table = lua_gettop(state);
        for (int i = 0; i < keyboards.size(); i++) {
            lua_pushstring(state, keyboards[i].c_str());
            lua_seti(state, keyboards_table, i + 1);
        }
        lua_pushliteral(state, "n");
        lua_pushinteger(state, keyboards.size());
        lua_settable(state, keyboards_table);
        return 1;
    } else {
        string error = SCREEN + ": no such method: ";
        error += method;
        std::cerr << error << std::endl;
        lua_pushstring(state, error.c_str());
        lua_error(state);
        return 0;
    }
}

std::vector<std::pair<string, bool>> Screen::get_methods() {
    std::vector<std::pair<string, bool>> result;
    result.push_back({"getKeyboards", 1});
    return result;
}

string Screen::get_type() {
    return SCREEN;
}

void Screen::update_size(int w, int h) {
    if (bg_buffer) {
        for (int i = 0; i < width; i++) {
            delete[] bg_buffer[i];
            delete[] fg_buffer[i];
            delete[] ch_buffer[i];
        }
        delete[] bg_buffer;
        delete[] fg_buffer;
        delete[] ch_buffer;
    }
    bg_buffer = new unsigned int *[w];
    fg_buffer = new unsigned int *[w];
    ch_buffer = new unsigned int *[w];
    for (int i = 0; i < w; i++) {
        bg_buffer[i] = new unsigned int[h];
        fg_buffer[i] = new unsigned int[h];
        ch_buffer[i] = new unsigned int[h];
    }
    width = w;
    height = h;
    viewport_width = w;
    viewport_height = h;
    SDL_SetWindowSize(window, w * SCREEN_FONT_WIDTH, h * SCREEN_FONT_HEIGHT);
    SDL_Rect rect;
    SDL_GetWindowSize(window, &rect.w, &rect.h);
    SDL_FillRect(surface, &rect, 0xFFFF);
    SDL_Delay(100);
    if (SDL_UpdateWindowSurface(window)) {
        std::cout << SDL_GetError() << "\n";
        surface = SDL_GetWindowSurface(window);
    }
}


static TTF_Font *font = nullptr;

void Screen::draw_char(int x, int y, unsigned int bg, unsigned int fg, unsigned int c) {
    if (font == nullptr) {
        font = TTF_OpenFont((project_dir + SCREEN_FONT_FILE).c_str(), 16);
        if (font == nullptr) {
            std::cerr << "font loading error: ";
            std::cerr << SDL_GetError() << std::endl;
            exit(1);
        }
    }
    bg |= 0xFF000000;
    unsigned int rb = (bg & 0xFF0000U) >> 16U;
    unsigned int gb = (bg & 0x00FF00U) >> 8U;
    unsigned int bb = (bg & 0x0000FFU);
    unsigned int rf = (fg & 0xFF0000U) >> 16U;
    unsigned int gf = (fg & 0x00FF00U) >> 8U;
    unsigned int bf = (fg & 0x0000FFU);
    SDL_Rect rect;
    rect.x = x * SCREEN_FONT_WIDTH;
    rect.y = y * SCREEN_FONT_HEIGHT;
    rect.w = SCREEN_FONT_WIDTH;
    rect.h = SCREEN_FONT_HEIGHT;
    SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, rb, gb, bb));
    SDL_Color color;
    color.r = rf;
    color.g = gf;
    color.b = bf;
    color.a = 0xFF;
    SDL_Surface *text_surface = TTF_RenderGlyph_Solid(font, c, color);
    SDL_BlitSurface(text_surface, NULL, surface, &rect);
    SDL_FreeSurface(text_surface);
}

void Screen::set_char(int x, int y, unsigned int bg, unsigned int fg, unsigned int c) {
    bg_buffer[x][y] = bg;
    fg_buffer[x][y] = fg;
    ch_buffer[x][y] = c;
    draw_char(x, y, bg, fg, c);
}

void Screen::update() {
    while (SDL_UpdateWindowSurface(window)) {
        surface = SDL_GetWindowSurface(window);
        std::cerr << "Screen::update(): " << SDL_GetError() << "\n";
    }
}

Screen::~Screen() {
    delete &keyboards;
    SDL_DestroyWindow(window);
}

Keyboard::Keyboard(const string &project_dir, const string &name) : Component(name, get_component_address(project_dir,
                                                                                                          KEYBOARD,
                                                                                                          name)) {

}

int Keyboard::invoke(const string &method, lua_State *state) {
    if (false) {
    } else {
        string error = KEYBOARD + ": no such method: ";
        error += method;
        std::cerr << error << std::endl;
        lua_pushstring(state, error.c_str());
        lua_error(state);
        return 0;
    }
}

std::vector<std::pair<string, bool>> Keyboard::get_methods() {
    std::vector<std::pair<string, bool>> result;
    return result;
}

string Keyboard::get_type() {
    return KEYBOARD;
}

Gpu::Gpu(const string &project_dir, const string &name) : Component(name,
                                                                    get_component_address(project_dir, GPU, name)) {
    std::ifstream in(get_component_folder(project_dir, GPU, name) + GPU_CONFIG_FILE);
    in >> color_depth >> max_width >> max_height;
}

int Gpu::invoke(const string &method, lua_State *state) {
    if (method == "bind") {
        if (lua_gettop(state) != 1 && lua_gettop(state) != 2) api_error(state, "bind(): invalid number of arguments");
        if (!lua_isstring(state, 1)) api_error(state, "bind(): invalid type of argument #1");
        string address = lua_tostring(state, 1);
        bool reset = false;
        if (lua_gettop(state) == 2) {
            reset = lua_toboolean(state, 2);
        }
        Component *component = computer->get_component(address);
        if (!component) {
            lua_pushboolean(state, false);
            lua_pushliteral(state, "no such component");
            return 2;
        }
        if (component->get_type() != SCREEN) {
            lua_pushboolean(state, false);
            lua_pushliteral(state, "component is not a screen");
            return 2;
        }
        screen = dynamic_cast<Screen *>(component);
        lua_pushboolean(state, true);
        return 1;
    } else if (method == "getResolution") {
        if (screen) {
            lua_pushinteger(state, screen->width);
            lua_pushinteger(state, screen->height);
            return 2;
        } else api_error(state, "getResolution(): unbound GPU");
    } else if (method == "setResolution") {
        if (screen) {
            if (lua_gettop(state) != 2) api_error(state, "setResolution(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "setResolution(): invalid type of argument #1");
            if (!lua_isnumber(state, 2)) api_error(state, "setResolution(): invalid type of argument #2");
            int w = lua_tonumber(state, 1);
            int h = lua_tonumber(state, 2);
            auto[max_w, max_h] = get_max_resolution();
            if (screen->width == w && screen->height == h) {
                lua_pushboolean(state, false);
                return 1;
            }
            if (w < 1 || w > max_w || h < 1 || h > max_h) api_error(state, "setResolution(): invalid resolution");
            screen->update_size(w, h);
            lua_pushboolean(state, true);
            return 1;
        } else api_error(state, "setResolution(): unbound GPU");
    } else if (method == "setBackground") {
        if (screen) {
            if (lua_gettop(state) != 1 && lua_gettop(state) != 2) api_error(state,
                                                                            "setBackground(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "setBackground(): invalid type of argument #1");
            bool palette = false;
            if (lua_gettop(state) == 2) {
                palette = lua_toboolean(state, 2);
            }
            if (palette) api_error(state, "setBackground(): palette is not implemented yet"); // TODO: implement palette
            int color = lua_tonumber(state, 1);
            int old_color = background_color;
            background_color = color;
            lua_pushinteger(state, old_color);
            return 1;
        } else api_error(state, "setBackground(): unbound GPU");
    } else if (method == "setForeground") {
        if (screen) {
            if (lua_gettop(state) != 1 && lua_gettop(state) != 2) api_error(state,
                                                                            "setForeground(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "setForeground(): invalid type of argument #1");
            bool palette = false;
            if (lua_gettop(state) == 2) {
                palette = lua_toboolean(state, 2);
            }
            if (palette) api_error(state, "setForeground(): palette is not implemented yet"); // TODO: implement palette
            int color = lua_tonumber(state, 1);
            int old_color = foreground_color;
            foreground_color = color;
            lua_pushinteger(state, old_color);
            return 1;
        } else api_error(state, "setForeground(): unbound GPU");
    } else if (method == "fill") {
        if (screen) {
            if (lua_gettop(state) != 5) api_error(state, "fill(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "fill(): invalid type of argument #1");
            if (!lua_isnumber(state, 2)) api_error(state, "fill(): invalid type of argument #2");
            if (!lua_isnumber(state, 3)) api_error(state, "fill(): invalid type of argument #3");
            if (!lua_isnumber(state, 4)) api_error(state, "fill(): invalid type of argument #4");
            if (!lua_isstring(state, 5)) api_error(state, "fill(): invalid type of argument #5");
            int x = lua_tonumber(state, 1);
            int y = lua_tonumber(state, 2);
            x--;
            y--;
            int w = lua_tonumber(state, 3);
            int h = lua_tonumber(state, 4);
            char c = lua_tostring(state, 5)[0];
            //printf("fill(): %d %d %d %d %d\n", x, y, w, h, c);
            if (x < 0 || x + w > screen->width || y < 0 || y + h > screen->height) {
                lua_pushboolean(state, false);
                return 1;
            }
            for (int cx = x; cx < x + w; cx++) {
                for (int cy = y; cy < y + h; cy++) {
                    screen->set_char(cx, cy, background_color, foreground_color, c);
                }
            }
            screen->update();
            lua_pushboolean(state, true);
            return 1;
        } else api_error(state, "fill(): unbound GPU");
    } else if (method == "set") {
        if (screen) {
            if (lua_gettop(state) != 3 && lua_gettop(state) != 4) api_error(state,
                                                                            "set(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "set(): invalid type of argument #1");
            if (!lua_isnumber(state, 2)) api_error(state, "set(): invalid type of argument #2");
            if (!lua_isstring(state, 3)) api_error(state, "set(): invalid type of argument #3");
            int x = lua_tonumber(state, 1);
            int y = lua_tonumber(state, 2);
            x--;
            y--;
            auto *s = lua_tostring(state, 3);
            auto *e = s + strlen(s);
            int l = utf8_length(s, e);
            bool vertical = false;
            if (lua_gettop(state) == 4) {
                vertical = lua_toboolean(state, 4);
            }
            if (vertical) {
                if (x < 0 || x >= screen->width || y < 0 || y + l > screen->height) {
                    lua_pushboolean(state, false);
                    return 1;
                }
                auto *p = s;
                utfint c;
                for (int i = 0; i < l; i++) {
                    utf8_decode(p, &c, 0);
                    screen->set_char(x, y + i, background_color, foreground_color, c);
                    p = utf8_next(p, e);
                }
            } else {
                if (x < 0 || x + l > screen->width || y < 0 || y >= screen->height) {
                    lua_pushboolean(state, false);
                    return 1;
                }
                auto *p = s;
                utfint c;
                for (int i = 0; i < l; i++) {
                    utf8_decode(p, &c, 0);
                    screen->set_char(x + i, y, background_color, foreground_color, c);
                    p = utf8_next(p, e);
                }
            }
            //SDL_Delay(125);
            screen->update();
            lua_pushboolean(state, true);
            return 1;
        } else api_error(state, "set(): unbound GPU");
    } else if (method == "get") {
        if (screen) {
            if (lua_gettop(state) != 2) api_error(state, "set(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "set(): invalid type of argument #1");
            if (!lua_isnumber(state, 2)) api_error(state, "set(): invalid type of argument #2");
            int x = lua_tonumber(state, 1);
            int y = lua_tonumber(state, 2);
            x--;
            y--;
            if (x < 0 || x >= screen->width || y < 0 || y >= screen->height) api_error(state,
                                                                                       "coordinates out of bounds");
            lua_pushstring(state, string(1, screen->ch_buffer[x][y]).c_str());
            lua_pushinteger(state, screen->fg_buffer[x][y]);
            lua_pushinteger(state, screen->bg_buffer[x][y]);
            return 3;
        } else api_error(state, "get(): unbound GPU");
    } else if (method == "getScreen") {
        if (screen) {
            lua_pushstring(state, screen->address.c_str());
            return 1;
        } else return 0;
    } else if (method == "maxResolution") {
        auto[w, h] = get_max_resolution();
        lua_pushinteger(state, w);
        lua_pushinteger(state, h);
        return 2;
    } else if (method == "getDepth") {
        lua_pushinteger(state, color_depth);
        return 1;
    } else if (method == "maxDepth") {
        lua_pushinteger(state, std::min(screen ? screen->color_depth : 24, color_depth));
        return 1;
    } else if (method == "setDepth") {
        if(lua_gettop(state) != 1) api_error(state, "setDepth(): invalid number of arguments");
        if(!lua_isnumber(state, 1)) api_error(state, "setDepth(): invalid type of argument #1");
        int depth = lua_tonumber(state, 1);
        // TODO: add depth support
        lua_pushboolean(state, 1);
        return 1;
    } else if (method == "getViewport") {
        if (screen) {
            lua_pushinteger(state, screen->viewport_width);
            lua_pushinteger(state, screen->viewport_height);
            return 2;
        } else api_error(state, "getViewport(): unbound GPU");
    } else if (method == "setViewport") {
        if (screen) {
            if (lua_gettop(state) != 2) api_error(state, "setViewport(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "setViewport(): invalid type of argument #1");
            if (!lua_isnumber(state, 2)) api_error(state, "setViewport(): invalid type of argument #2");
            int w = lua_tonumber(state, 1);
            int h = lua_tonumber(state, 2);
            if (screen->viewport_width == w && screen->viewport_height == h) {
                lua_pushboolean(state, false);
                return 1;
            }
            if (w < 1 || h < 1) api_error(state, "setViewport(): invalid viewport");
            screen->viewport_width = w;
            screen->viewport_height = h;
            lua_pushboolean(state, true);
            screen->update_size(screen->width, screen->height);
            return 1;
        } else api_error(state, "setViewport(): unbound GPU");
    } else if (method == "copy") {
        if (screen) {
            if (lua_gettop(state) != 6) api_error(state, "copy(): invalid number of arguments");
            if (!lua_isnumber(state, 1)) api_error(state, "copy(): invalid type of argument #1");
            if (!lua_isnumber(state, 2)) api_error(state, "copy(): invalid type of argument #2");
            if (!lua_isnumber(state, 3)) api_error(state, "copy(): invalid type of argument #3");
            if (!lua_isnumber(state, 4)) api_error(state, "copy(): invalid type of argument #4");
            if (!lua_isnumber(state, 5)) api_error(state, "copy(): invalid type of argument #5");
            if (!lua_isnumber(state, 6)) api_error(state, "copy(): invalid type of argument #6");
            int x1 = lua_tonumber(state, 1);
            int y1 = lua_tonumber(state, 2);
            x1--;
            y1--;
            int w = lua_tonumber(state, 3);
            int h = lua_tonumber(state, 4);
            int tx = lua_tonumber(state, 5);
            int ty = lua_tonumber(state, 6);
            //printf("copy(): %d %d %d %d %d %d\n", x1, y1, w, h, tx, ty);
            int c = 0;
            unsigned int **tmp_bg_buf;
            unsigned int **tmp_fg_buf;
            unsigned int **tmp_ch_buf;
            if(w > 0 && h > 0) {
                tmp_bg_buf = new unsigned int*[w];
                tmp_fg_buf = new unsigned int*[w];
                tmp_ch_buf = new unsigned int*[w];
                for(int i = 0; i < w; i++) {
                    tmp_bg_buf[i] = new unsigned int[h];
                    tmp_fg_buf[i] = new unsigned int[h];
                    tmp_ch_buf[i] = new unsigned int[h];
                }
                for (int cx = x1; cx < x1 + w; cx++) {
                    for (int cy = y1; cy < y1 + h; cy++) {
                        tmp_bg_buf[cx - x1][cy - y1] = screen->bg_buffer[cx][cy];
                        tmp_fg_buf[cx - x1][cy - y1] = screen->fg_buffer[cx][cy];
                        tmp_ch_buf[cx - x1][cy - y1] = screen->ch_buffer[cx][cy];
                    }
                }
            }
            for (int cx = x1; cx < x1 + w; cx++) {
                for (int cy = y1; cy < y1 + h; cy++) {
                    int dx = cx + tx;
                    int dy = cy + ty;
                    if (dx >= 0 && dx < screen->width && dy >= 0 && dy < screen->height &&
                        cx >= 0 && cx < screen->width && cy >= 0 && cy < screen->height) {
                        screen->set_char(dx, dy, tmp_bg_buf[cx - x1][cy - y1], tmp_fg_buf[cx - x1][cy - y1], tmp_ch_buf[cx - x1][cy - y1]);
                        //screen->set_char(dx, dy, screen->bg_buffer[cx][cy], screen->fg_buffer[cx][cy], screen->ch_buffer[cx][cy]);
                        c++;
                    }
                }
            }
            if(w > 0 && h > 0) {
                for(int i = 0; i < w; i++) {
                    delete[] tmp_bg_buf[i];
                    delete[] tmp_fg_buf[i];
                    delete[] tmp_ch_buf[i];
                }
                delete[] tmp_bg_buf;
                delete[] tmp_fg_buf;
                delete[] tmp_ch_buf;
            }
            screen->update();
            lua_pushboolean(state, c > 0);
            return 1;
        } else api_error(state, "copy(): unbound GPU");
    } else if (method == "getBackground") {
        lua_pushinteger(state, background_color);
        return 1;
    } else if (method == "getForeground") {
        lua_pushinteger(state, foreground_color);
        return 1;
    } else {
        string error = GPU + ": no such method: ";
        error += method;
        std::cerr << error << std::endl;
        lua_pushstring(state, error.c_str());
        lua_error(state);
        return 0;
    }
}

std::vector<std::pair<string, bool>> Gpu::get_methods() {
    std::vector<std::pair<string, bool>> result;
    result.push_back({"bind", 1});
    result.push_back({"getResolution", 1});
    result.push_back({"setResolution", 1});
    result.push_back({"setBackground", 1});
    result.push_back({"setForeground", 1});
    result.push_back({"getBackground", 1});
    result.push_back({"getForeground", 1});
    result.push_back({"fill", 1});
    result.push_back({"set", 1});
    result.push_back({"get", 1});
    result.push_back({"getScreen", 1});
    result.push_back({"maxResolution", 1});
    result.push_back({"getDepth", 1});
    result.push_back({"maxDepth", 1});
    result.push_back({"setDepth", 1});
    result.push_back({"getViewport", 1});
    result.push_back({"setViewport", 1});
    result.push_back({"copy", 1});
    return result;
}

string Gpu::get_type() {
    return GPU;
}

std::pair<int, int> Gpu::get_max_resolution() const {
    int w = max_width;
    int h = max_height;
    if (screen) {
        w = std::min(w, screen->max_width);
        h = std::min(h, screen->max_height);
    }
    return {w, h};
}

Gpu::~Gpu() = default;

ComputerComponent::ComputerComponent(Computer *computer) : Component(computer->name, computer->address) {

}

int ComputerComponent::invoke(const string &method, lua_State *state) {
    if (false) {
    } else {
        string error = COMPUTER + ": no such method: ";
        error += method;
        std::cerr << error << std::endl;
        lua_pushstring(state, error.c_str());
        lua_error(state);
        return 0;
    }
}

std::vector<std::pair<string, bool>> ComputerComponent::get_methods() {
    std::vector<std::pair<string, bool>> result;
    return result;
}

string ComputerComponent::get_type() {
    return COMPUTER;
}

Internet::Internet(const string &project_dir, const string &name) : Component(name, get_component_address(project_dir, INTERNET, name)){

}

int Internet::invoke(const string &method, lua_State *state) {
    if (false) {
    } else {
        string error = INTERNET + ": no such method: ";
        error += method;
        std::cerr << error << std::endl;
        lua_pushstring(state, error.c_str());
        lua_error(state);
        return 0;
    }
}

std::vector<std::pair<string, bool>> Internet::get_methods() {
    std::vector<std::pair<string, bool>> methods;
    return methods;
}

string Internet::get_type() {
    return INTERNET;
}

#pragma clang diagnostic pop