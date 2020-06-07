#ifndef LINUXINPUT_H_INCLUDED
#define LINUXINPUT_H_INCLUDED

// Linux async input routines
// To use this, your user must be in "input" group:
// sudo usermod -aG input <username>

#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>

// Start key input poll
FILE* start_input()
{
    return fopen("/dev/input/by-path/platform-i8042-serio-0-event-kbd", "rb");
}

// Retrieve all keys
void read_keys(FILE* input, char* key_map)
{
    memset(key_map, 0, KEY_MAX/8 + 1);    //  Initate the array to zero's
    ioctl(fileno(input), EVIOCGKEY(KEY_MAX/8 + 1), key_map);
}

char* get_all_keys(FILE* input)
{
    char* key_map = (char*)malloc(KEY_MAX/8 + 1);    //  Create a byte array the size of the number of keys

    read_keys(input, key_map);

    return key_map;
}

// Retrieve specific key
int check_key_state(char* key_map, int key)
{
    int keyb = key_map[key/8];  //  The key we want (and the seven others arround it)
    int mask = 1 << (key % 8);  //  Put a one in the same column as out key state will be in;

    return (keyb & mask) != 0;  //  Returns true if pressed otherwise false
}

int get_key_state(FILE* input, int key)
{
    char key_map[KEY_MAX/8 + 1];

    read_keys(input, key_map);

    return check_key_state(key_map, key);
}

void stop_input(FILE* input)
{
    fclose(input);
}

#endif // LINUXINPUT_H_INCLUDED
