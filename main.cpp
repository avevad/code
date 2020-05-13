#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-emplace"
#pragma ide diagnostic ignored "cert-msc51-cpp"
#pragma ide diagnostic ignored "cert-msc50-cpp"
#pragma ide diagnostic ignored "modernize-use-nullptr"
#pragma ide diagnostic ignored "cert-err58-cpp"

#include <iostream>
#include <map>
#include <vector>
#include <list>

#include "components.cpp"
#include "computer.cpp"
#include "lua_bridge.cpp"

#include "lua5.3/lua.h"
#include "SDL2/SDL.h"

using std::string;


static const string DEFAULT_USER = "user";

static void put_key_codes(std::map<SDL_Scancode, int> &key_codes) {
    key_codes[SDL_SCANCODE_1] = 0x02;
    key_codes[SDL_SCANCODE_2] = 0x03;
    key_codes[SDL_SCANCODE_3] = 0x04;
    key_codes[SDL_SCANCODE_4] = 0x05;
    key_codes[SDL_SCANCODE_5] = 0x06;
    key_codes[SDL_SCANCODE_6] = 0x07;
    key_codes[SDL_SCANCODE_7] = 0x08;
    key_codes[SDL_SCANCODE_8] = 0x09;
    key_codes[SDL_SCANCODE_9] = 0x0A;
    key_codes[SDL_SCANCODE_0] = 0x0B;
    key_codes[SDL_SCANCODE_A] = 0x1E;
    key_codes[SDL_SCANCODE_B] = 0x30;
    key_codes[SDL_SCANCODE_C] = 0x2E;
    key_codes[SDL_SCANCODE_D] = 0x20;
    key_codes[SDL_SCANCODE_E] = 0x12;
    key_codes[SDL_SCANCODE_F] = 0x21;
    key_codes[SDL_SCANCODE_G] = 0x22;
    key_codes[SDL_SCANCODE_H] = 0x23;
    key_codes[SDL_SCANCODE_I] = 0x17;
    key_codes[SDL_SCANCODE_J] = 0x24;
    key_codes[SDL_SCANCODE_K] = 0x25;
    key_codes[SDL_SCANCODE_L] = 0x26;
    key_codes[SDL_SCANCODE_M] = 0x32;
    key_codes[SDL_SCANCODE_N] = 0x31;
    key_codes[SDL_SCANCODE_O] = 0x18;
    key_codes[SDL_SCANCODE_P] = 0x19;
    key_codes[SDL_SCANCODE_Q] = 0x10;
    key_codes[SDL_SCANCODE_R] = 0x13;
    key_codes[SDL_SCANCODE_S] = 0x1F;
    key_codes[SDL_SCANCODE_T] = 0x14;
    key_codes[SDL_SCANCODE_U] = 0x16;
    key_codes[SDL_SCANCODE_V] = 0x2F;
    key_codes[SDL_SCANCODE_W] = 0x11;
    key_codes[SDL_SCANCODE_X] = 0x2D;
    key_codes[SDL_SCANCODE_Y] = 0x15;
    key_codes[SDL_SCANCODE_Z] = 0x2C;

    key_codes[SDL_SCANCODE_LSHIFT] = 0x2A;
    key_codes[SDL_SCANCODE_LCTRL] = 0x1D;
    key_codes[SDL_SCANCODE_BACKSPACE] = 0x0E;
    key_codes[SDL_SCANCODE_RETURN] = 0x1C;

    key_codes[SDL_SCANCODE_UP] = 0xC8;
    key_codes[SDL_SCANCODE_DOWN] = 0xD0;
    key_codes[SDL_SCANCODE_LEFT] = 0xCB;
    key_codes[SDL_SCANCODE_RIGHT] = 0xCD;
    key_codes[SDL_SCANCODE_HOME] = 0xC7;
    key_codes[SDL_SCANCODE_END] = 0xCF;
    key_codes[SDL_SCANCODE_DELETE] = 0xD3;

    key_codes[SDL_SCANCODE_F1] = 0x3B;
    key_codes[SDL_SCANCODE_F2] = 0x3C;
    key_codes[SDL_SCANCODE_F3] = 0x3E;
    key_codes[SDL_SCANCODE_F4] = 0x3F;
    key_codes[SDL_SCANCODE_F5] = 0x40;
    key_codes[SDL_SCANCODE_F6] = 0x41;
    key_codes[SDL_SCANCODE_F7] = 0x42;
    key_codes[SDL_SCANCODE_F8] = 0x43;
    key_codes[SDL_SCANCODE_F9] = 0x44;
    key_codes[SDL_SCANCODE_F10] = 0x45;
    key_codes[SDL_SCANCODE_F11] = 0x57;
    key_codes[SDL_SCANCODE_F12] = 0x58;
    key_codes[SDL_SCANCODE_F13] = 0x64;
    key_codes[SDL_SCANCODE_F14] = 0x65;
    key_codes[SDL_SCANCODE_F15] = 0x66;
    key_codes[SDL_SCANCODE_F16] = 0x67;
    key_codes[SDL_SCANCODE_F17] = 0x68;
    key_codes[SDL_SCANCODE_F18] = 0x69;
    key_codes[SDL_SCANCODE_F19] = 0x71;
}


static void sdl_poll_event_thread(Computer *computer) {
    std::map<SDL_Scancode, int> key_codes;
    put_key_codes(key_codes);

    bool ctrl = false;

    std::vector<Component *> components;
    computer->get_components(&components);
    while (true) {
        SDL_Event event;
        bool ok = SDL_WaitEvent(&event);
        if(!ok) {
            std::cerr << "Failed to wait for SDL event: " << SDL_GetError() << std::endl;
            return;
        }
        if(event.type == SDL_QUIT) {
            exit(0);
        } else if(event.type == SDL_KEYDOWN) {
            //printf("key: %c %d\n", event.key.keysym.sym, event.key.keysym.scancode);
            utfint key_char = event.key.keysym.sym;
            if(key_char > 0xFFFF) key_char = 0;
            if(key_char) {
                //printf("%d\n", key_char);
                if(!ctrl && key_char != 13 && key_char != '\b' && key_char != 127) { // enter, backspace and delete
                    SDL_Event event2;
                    do {
                        SDL_WaitEvent(&event2);
                    } while (event2.type != SDL_TEXTINPUT && event2.type != SDL_QUIT);
                    if (event2.type == SDL_QUIT) exit(0);
                    utf8_decode(event2.text.text, &key_char, 0);
                }
            };
            if(event.key.keysym.scancode == SDL_SCANCODE_LCTRL || event.key.keysym.scancode == SDL_SCANCODE_RCTRL) ctrl = true;
            for(Component *component : components) {
                if(component->get_type() == SCREEN) {
                    auto *screen = dynamic_cast<Screen *>(component);
                    if(SDL_GetWindowID(screen->window) == event.window.windowID) {
                        if(screen->keyboards.empty()) break;
                        string keyboard = screen->keyboards[0];
                        string signal = "\"";
                        signal += "key_down";
                        signal += "\", \"";
                        signal += keyboard;
                        signal += "\", ";
                        signal += std::to_string(key_char);
                        signal += ", ";
                        signal += std::to_string(key_codes[event.key.keysym.scancode]);
                        signal += ", \"";
                        signal += DEFAULT_USER;
                        signal += "\"";
                        std::unique_lock<std::mutex> locker(computer->queue_lock);
                        computer->signal_queue.push(signal);
                        computer->queue_notifier.notify_all();
                        break;
                    }
                }
            }
        } else if(event.type == SDL_KEYUP){
            for(Component *component : components) {
                if(component->get_type() == SCREEN) {
                    if(event.key.keysym.scancode == SDL_SCANCODE_LCTRL || event.key.keysym.scancode == SDL_SCANCODE_RCTRL) ctrl = false;
                    auto *screen = dynamic_cast<Screen *>(component);
                    if(SDL_GetWindowID(screen->window) == event.window.windowID) {
                        if(screen->keyboards.empty()) break;
                        string keyboard = screen->keyboards[0];
                        string signal = "\"";
                        signal += "key_up";
                        signal += "\", \"";
                        signal += keyboard;
                        signal += "\", ";
                        int key_code = event.key.keysym.sym;
                        if(key_code > 0xFFFF) key_code = 0;
                        signal += std::to_string(key_code);
                        signal += ", ";
                        signal += std::to_string(key_codes[event.key.keysym.scancode]);
                        signal += ", \"";
                        signal += DEFAULT_USER;
                        signal += "\"";
                        std::unique_lock<std::mutex> locker(computer->queue_lock);
                        computer->signal_queue.push(signal);
                        computer->queue_notifier.notify_all();
                        break;
                    }
                }
            }
        }
    }
}


void exec_cmd(string &project_directory, std::list<string> &cmd_tokens) {
    std::map<string, Component *> components;
    Component::load_components(project_directory, components);
    if (cmd_tokens.empty()) return;
    string cmd;
    cmd = cmd_tokens.front();
    cmd_tokens.pop_front();
    if(cmd == "start") {
        if (cmd_tokens.empty()) {
            printf("Not enough arguments\n");
            return;
        }
        string computer_name = cmd_tokens.front();
        cmd_tokens.pop_front();
        auto *computer = new Computer(project_directory, computer_name, components);
        std::thread event_thread(sdl_poll_event_thread, computer);
        std::thread *thread = boot_computer(computer);
        thread->join();

        SDL_Event quit_event;
        quit_event.type = SDL_QUIT; // signalling thread to terminate
        SDL_PushEvent(&quit_event);
        event_thread.join(); // waiting for thread to terminate

        delete thread;
        delete computer;
        for(auto [name, component] : components) delete component;
        components.clear();
    }
}

int main(int argc, char **argv) {
    srand(time(nullptr));
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    //SDL_EnableUNICODE(1);
    if (TTF_Init()) {
        std::cerr << "Failed to initialize TTF: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (argc < 2) {
        printf("No project directory specified\n");
        return 1;
    }
    string project_directory = string(argv[1]) + "/";
    std::list<string> cmd_tokens;
    for(int i = 2; i < argc; i++) cmd_tokens.push_back(string(argv[i]));
    exec_cmd(project_directory, cmd_tokens);
    SDL_Quit();
    return 0;
}

#pragma clang diagnostic pop