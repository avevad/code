#ifndef CODE_COMPONENTS_H
#define CODE_COMPONENTS_H

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <queue>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "computer.h"

using std::string;

static const string COMPONENTS_FOLDER = "components/";
static const string COMPONENT_TYPE_DELIMITER = ".";
static const string COMPONENT_ADDRESS_FILE = "/address.txt";

static string
get_component_folder(const string &project_dir, const string &component_type, const string &component_name);

static string
get_component_address(const string &project_dir, const string &component_type, const string &component_name);

class Computer;

struct lua_State;

class Component {
public:
    Computer *computer = nullptr;
    const string address;
    const string name;

    explicit Component(string name, string address);

    virtual int invoke(const string &method, lua_State *state) = 0;

    virtual std::vector<std::pair<string, bool>> get_methods() = 0;

    virtual string get_type() = 0;

    virtual ~Component();

    static int load_components(const string &project_dir, std::map<string, Component *> &components);
};

static const string COMPUTER = "computer";

class ComputerComponent : public Component {
public:
    explicit ComputerComponent(Computer *computer);

    int invoke(const string &method, lua_State *state) override;

    std::vector<std::pair<string, bool>> get_methods() override;

    string get_type() override;
};

static const string EEPROM = "eeprom";
static const string EEPROM_PRIMARY_DATA_FILE = "/primary.lua";
static const size_t EEPROM_MAX_PRIMARY_SIZE = 4096;
static const string EEPROM_SECONDARY_DATA_FILE = "/secondary.bin";
static const size_t EEPROM_MAX_SECONDARY_SIZE = 256;
static const string EEPROM_LABEL_FILE = "/label.txt";

class Eeprom : public Component {
public:
    const string project_dir;

    Eeprom(const string &project_dir, const string &name);

    string get_primary();

    string get_secondary();

    int invoke(const string &method, lua_State *state) override;

    std::vector<std::pair<string, bool>> get_methods() override;

    string get_type() override;

    ~Eeprom();
};


static const string FILESYSTEM = "filesystem";
static const string FILESYSTEM_DATA_FOLDER = "/data/";
static const string FILESYSTEM_READONLY_MARKER = "/readonly.txt";
static const string FILESYSTEM_READONLY_LABEL_FILE = "/label.txt";
static const int FILESYSTEM_MAX_BUFFER_SIZE = 4096;

class Filesystem : public Component {
    class Descriptor {
    public:
        std::fstream *const stream;

        explicit Descriptor(std::fstream *stream);

        ~Descriptor();
    };

private:
    std::vector<Descriptor *> descriptors;
    std::queue<int> free_descriptors;
public:
    const string project_dir;

    Filesystem(const string &project_dir, const string &name);

    int invoke(const string &method, lua_State *state) override;

    std::vector<std::pair<string, bool>> get_methods() override;

    string get_type() override;

    string get_data_directory();

    bool is_readonly();

    string get_label();

    void set_label(const string &label);

    unsigned long long space_used();

    ~Filesystem();
};


static const string SCREEN = "screen";
static const string SCREEN_CONFIG_FILE = "/config.txt";
static const string SCREEN_KEYBOARDS_FILE = "/keyboards.txt";
static const string SCREEN_FONT_FILE = "font.ttf";
static const int SCREEN_FONT_WIDTH = 8, SCREEN_FONT_HEIGHT = 16;

class Screen : public Component {
public:
    const string project_dir;
    int color_depth;
    int max_width, max_height;
    int width, height;
    int ratio_width, ratio_height;
    int viewport_width, viewport_height;
    std::vector<string> keyboards;
    SDL_Window *const window;
    SDL_Surface *surface;
    unsigned int **ch_buffer = nullptr;
    unsigned int **fg_buffer = nullptr;
    unsigned int **bg_buffer = nullptr;

    Screen(const string &project_dir, const string &name);

    int invoke(const string &method, lua_State *state) override;

    std::vector<std::pair<string, bool>> get_methods() override;

    string get_type() override;

    void update_size(int w, int h);

    void set_char(int x, int y, unsigned int bg, unsigned int fg, unsigned int c);

    void draw_char(int x, int y, unsigned int bg, unsigned int fg, unsigned int c);

    void update();

    ~Screen();
};

static const string KEYBOARD = "keyboard";

class Keyboard : public Component {
public:
    Keyboard(const string &project_dir, const string &name);

    int invoke(const string &method, lua_State *state) override;

    std::vector<std::pair<string, bool>> get_methods() override;

    string get_type() override;
};

static const string GPU = "gpu";
static const string GPU_CONFIG_FILE = "/config.txt";

class Gpu : public Component {
public:
    int color_depth;
    int max_width, max_height;
    int background_color = 0x000000, foreground_color = 0xFFFFFF;
    Screen *screen = nullptr;

    Gpu(const string &project_dir, const string &name);

    int invoke(const string &method, lua_State *state) override;

    std::vector<std::pair<string, bool>> get_methods() override;

    string get_type() override;

    std::pair<int, int> get_max_resolution() const;

    ~Gpu();
};

static const string INTERNET = "internet";

class Internet : public Component {
public:
    Internet(const string &project_dir, const string &name);

    int invoke(const string &method, lua_State *state) override;

    std::vector<std::pair<string, bool>> get_methods() override;

    string get_type() override;
};

#endif //CODE_COMPONENTS_H

