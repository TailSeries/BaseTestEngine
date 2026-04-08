#pragma once

#include <stdio.h>

#ifndef LogStringMsg
#if PLATFORM_WIN
#include <Windows.h>
#define LogStringMsg(...)  do{ static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE);  SetConsoleTextAttribute(__LoghConsole, 7); printf(__VA_ARGS__);printf("\n"); SetConsoleTextAttribute(__LoghConsole, 7);}while(0);
#define DebugStringMsg(...) do{static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE); SetConsoleTextAttribute(__LoghConsole, 15); printf(__VA_ARGS__); printf("\n");SetConsoleTextAttribute(__LoghConsole, 7);}while(0);
#define WarningStringMsg(...) do{static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE); SetConsoleTextAttribute(__LoghConsole, 14); printf(__VA_ARGS__); printf("\n");SetConsoleTextAttribute(__LoghConsole, 7);}while(0);
#define ErrorStringMsg(...)  do{static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE); SetConsoleTextAttribute(__LoghConsole, 12); printf(__VA_ARGS__); printf("\n");SetConsoleTextAttribute(__LoghConsole, 7);}while(0);

#define LogTraceMsg(...)  do{ static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE);  SetConsoleTextAttribute(__LoghConsole, 7); printf("[log D]%s#%d: ", __FILE__,__LINE__); printf(__VA_ARGS__); SetConsoleTextAttribute(__LoghConsole, 7);}while(0);
#define DebugTraceMsg(...) do{static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE); SetConsoleTextAttribute(__LoghConsole, 15); printf("[log I]%s#%d: ", __FILE__,__LINE__); printf(__VA_ARGS__); SetConsoleTextAttribute(__LoghConsole, 7);}while(0);
#define WarningTraceMsg(...) do{static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE); SetConsoleTextAttribute(__LoghConsole, 14);printf("[log W]%s#%d: ", __FILE__,__LINE__); printf(__VA_ARGS__); SetConsoleTextAttribute(__LoghConsole, 7);}while(0);
#define ErrorTraceMsg(...)  do{static HANDLE __LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE); SetConsoleTextAttribute(__LoghConsole, 12);printf("[log E]%s#%d: ", __FILE__,__LINE__); printf(__VA_ARGS__); SetConsoleTextAttribute(__LoghConsole, 7);}while(0);


#else
#define NONE         "\e[m"
#define RED          "\e[0;32;31m"
#define LIGHT_RED    "\e[1;31m"
#define GREEN        "\e[0;32;32m"
#define LIGHT_GREEN  "\e[1;32m"
#define BLUE         "\e[0;32;34m"
#define LIGHT_BLUE   "\e[1;34m"
#define DARY_GRAY    "\e[1;30m"
#define CYAN         "\e[0;36m"
#define LIGHT_CYAN   "\e[1;36m"
#define PURPLE       "\e[0;35m"
#define LIGHT_PURPLE "\e[1;35m"
#define BROWN        "\e[0;33m"
#define YELLOW       "\e[1;33m"
#define LIGHT_GRAY   "\e[0;37m"
#define WHITE        "\e[1;37m"

#define LogStringMsg(...)  do{printf(DARY_GRAY "[log D]%s#%d: " NONE, __FILE__,__LINE__); printf(__VA_ARGS__);}while(0)
#define DebugStringMsg(...) do{printf(GREEN "[log I]%s#%d: " NONE, __FILE__,__LINE__); printf(__VA_ARGS__);}while(0)
#define WarningStringMsg(...) do{printf(YELLOW "[log W]%s#%d: " NONE, __FILE__,__LINE__); printf(__VA_ARGS__);}while(0)
#define ErrorStringMsg(...)  do{printf(LIGHT_RED "[log E]%s#%d: " NONE, __FILE__,__LINE__); printf(__VA_ARGS__);}while(0)

#endif
#endif

