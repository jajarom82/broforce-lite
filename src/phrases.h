// Broyoo - on-screen texts and matching voice lines for the level intro and the mission-failed screens
#pragma once
#include "audio.h"

constexpr int INTRO_HOLD = 170;   // ticks of black screen with the level title (game frozen)
constexpr int INTRO_FADE = 70;    // ticks to fade the black away while the game starts
constexpr int FAIL_FADE = 90;     // ticks to fade to black on mission failed
constexpr int FAIL_MENU = 160;    // ticks before the retry / quit menu appears

constexpr int NUM_INTRO = 5;
static const char *const INTRO_TXT[NUM_INTRO] = {"GO, GO, GO!", "LET'S GO, BRO!", "HERE WE GO!", "LOCK AND LOAD!", "NO MERCY!"};
static const int INTRO_SP[NUM_INTRO] = {SP_GOGOGO, SP_LETSGO, SP_HEREWEGO, SP_LOCKLOAD, SP_NOMERCY};

constexpr int NUM_FAIL = 3;
static const char *const FAIL_TXT[NUM_FAIL] = {"", "REST IN PEACE, BRO", "NOOOOOOOOO!"};
static const int FAIL_SP[NUM_FAIL] = {SP_MISSIONFAILED, SP_RIP, SP_NOOO};
