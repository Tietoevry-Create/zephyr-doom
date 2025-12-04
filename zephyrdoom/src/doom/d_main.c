//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//      plus functions to determine game mode (shareware, registered),
//      parse command line parameters, configure game parameters (turbo),
//      and call the startup functions.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SEGGER
#include <strings.h>

#endif

#include <zephyr/kernel.h>

#include "am_map.h"
#include "d_iwad.h"
#include "d_main.h"
#include "deh_main.h"
#include "doom_config.h"
#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_endoom.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_menu.h"
#include "m_misc.h"
#include "n_fs.h"
#include "n_rjoy.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "net_query.h"
#include "nrf.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "r_local.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "statdump.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_main.h"
#include "w_wad.h"
#include "wi_stuff.h"
#include "z_zone.h"

extern int no_sdcard;  // NRFD-NOTE: from main.c
//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop(void);

// Location where savegames are stored

char* savegamedir;

// location of IWAD and WAD files

char* iwadfile;

boolean devparm;      // started game with -devparm
boolean nomonsters;   // checkparm of -nomonsters
boolean respawnparm;  // checkparm of -respawn
boolean fastparm;     // checkparm of -fast

extern boolean inhelpscreens;

skill_t startskill;
int startepisode;
int startmap;
boolean autostart;
int startloadgame;

boolean advancedemo;

// If true, the main game loop has started.
boolean main_loop_started = false;

const int show_endoom = 1;
const int show_diskicon = 1;

uint32_t frame_time_prev;
uint32_t frame_time_fps;

void D_ConnectNetGame(void);
void D_CheckNetGame(void);

//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents(void) {
    event_t* ev;

    while ((ev = D_PopEvent()) != NULL) {
        if (M_Responder(ev)) continue;  // menu ate the event
        G_Responder(ev);
    }
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;
extern boolean setsizeneeded;
extern const int showMessages;
void R_ExecuteSetViewSize(void);

void D_Display(void) {
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static boolean inhelpscreensstate = false;
    static boolean fullscreen = false;
    static gamestate_t oldgamestate = -1;
    static int borderdrawcount;
    int nowtime;
    int tics;
    int wipestart;
    int y;
    boolean done;
    boolean wipe;
    boolean redrawsbar;

    if (nodrawers) return;  // for comparative timing / profiling

    redrawsbar = false;

    // change the view size if needed
    if (setsizeneeded) {
        R_ExecuteSetViewSize();
        oldgamestate = -1;  // force background redraw
        borderdrawcount = 3;
    }

    // save the current screen if about to wipe
    if (gamestate != wipegamestate) {
        wipe = true;
        wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    } else
        wipe = false;

    I_ClearVideoBuffer();

    if (gamestate == GS_LEVEL && gametic) HU_Erase();

    // do buffered drawing
    switch (gamestate) {
        case GS_LEVEL:
            if (!gametic) break;
            if (automapactive) AM_Drawer();
            if (wipe || (viewheight != SCREENHEIGHT && fullscreen))
                redrawsbar = true;
            if (inhelpscreensstate && !inhelpscreens)
                redrawsbar = true;  // just put away the help screen
            ST_Drawer(viewheight == SCREENHEIGHT, redrawsbar);
            fullscreen = viewheight == SCREENHEIGHT;
            break;

        case GS_INTERMISSION:
            WI_Drawer();
            break;

        case GS_FINALE:
            F_Drawer();
            break;

        case GS_DEMOSCREEN:
            D_PageDrawer();
            break;
    }

    // draw buffered stuff to screen
    I_UpdateNoBlit();

    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
        R_RenderPlayerView(&players[displayplayer]);

    if (gamestate == GS_LEVEL && gametic) HU_Drawer();

    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL) {
        I_SetPalette(W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE));
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    // draw pause pic
    if (paused) {
        N_ldbg("D_Display: Print pause pic\n");
        if (automapactive)
            y = 4;
        else
            y = viewwindowy + 4;
        V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y,
                          W_CacheLumpName(DEH_String("M_PAUSE"), PU_CACHE));
    }

    // menus go directly to the screen
    M_Drawer();  // menu is drawn even on top of everything

    // normal update
    if (!wipe) {
        I_FinishUpdate();  // page flip or blit buffer
        return;
    }

    // wipe update
    wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

    wipestart = I_GetTime() - 1;

    tics = 0;
    do {
        do {
            nowtime = I_GetTime();
            tics = nowtime - wipestart;
            I_UpdateSound();
            I_Sleep(1);
        } while (tics <= 0);
        wipestart = nowtime;
        done =
            wipe_ScreenWipe(wipe_Melt, 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
        I_UpdateNoBlit();
        M_Drawer();        // menu is drawn even on top of wipes
        I_FinishUpdate();  // page flip or blit buffe
        I_UpdateSound();

    } while (!done);
}

static void EnableLoadingDisk(void) {
    char* disk_lump_name;

    if (show_diskicon) {
        disk_lump_name = DEH_String("STDISK");
        V_EnableLoadingDisk(disk_lump_name, SCREENWIDTH - LOADING_DISK_W,
                            SCREENHEIGHT - LOADING_DISK_H);
    }
}

//
// Add configuration file variable bindings.
//

void D_BindVariables(void) {
    int i;

    M_ApplyPlatformDefaults();

    I_BindInputVariables();
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindBaseControls();
    M_BindWeaponControls();
    M_BindMapControls();
    M_BindMenuControls();
    M_BindChatControls(MAXPLAYERS);

    NET_BindVariables();

    M_BindIntVariable("sfx_volume", &sfxVolume);
    M_BindIntVariable("music_volume", &musicVolume);
}

//
//  D_DoomLoop
//
//

void D_DoomLoop(void) {
    printf("D_DoomLoop\n");
    main_loop_started = true;

    I_SetWindowTitle(gamedescription);
    I_GraphicsCheckCommandLine();
    I_InitGraphics();
    EnableLoadingDisk();

    TryRunTics();

    V_RestoreBuffer();
    R_ExecuteSetViewSize();

    D_StartGameLoop();

    frame_time_prev = I_GetTimeRaw();

    // Game keeps restarting at startup without this.
    // Some device initialized above probably needs time to set up
    k_msleep(2);

    while (1) {
        k_usleep(10);
        // nrf_cache_profiling_counters_clear(NRF_CACHE_S);
        int frame_time = I_GetTimeRaw();
        frame_time_fps = I_RawTimeToFps(frame_time - frame_time_prev);

        // frame syncronous IO operations
        I_StartFrame();

        TryRunTics();  // will run at least one tic

        S_UpdateSounds(players[consoleplayer].mo);  // move positional sounds

        // printk("FPS: %d\n", frame_time_fps);

        // Update display, next frame, with current state.
        if (screenvisible) D_Display();

        N_ldbg("=== LOOP END ===\n");
        frame_time_prev = frame_time;
    }
}

//
//  DEMO LOOP
//
int demosequence;
int pagetic;
char* pagename;

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void) {
    if (--pagetic < 0) D_AdvanceDemo();
}

//
// D_PageDrawer
//
void D_PageDrawer(void) {
    // NRFD-TODO:
    N_ldbg("D_PageDrawer %s\n", pagename);
    V_DrawPatch(0, 0, W_CacheLumpName(pagename, PU_CACHE));
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo(void) { advancedemo = true; }

//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
void D_DoAdvanceDemo(void) {
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;  // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    // The Ultimate Doom executable changed the demo sequence to add
    // a DEMO4 demo.  Final Doom was based on Ultimate, so also
    // includes this change; however, the Final Doom IWADs do not
    // include a DEMO4 lump, so the game bombs out with an error
    // when it reaches this point in the demo sequence.

    // However! There is an alternate version of Final Doom that
    // includes a fixed executable.

    if (gameversion == exe_ultimate || gameversion == exe_final)
        demosequence = (demosequence + 1) % 7;
    else
        demosequence = (demosequence + 1) % 6;

    switch (demosequence) {
        case 0:
            if (gamemode == commercial)
                pagetic = TICRATE * 11;
            else
                pagetic = 170;
            gamestate = GS_DEMOSCREEN;
            pagename = DEH_String("TITLEPIC");
            if (gamemode == commercial)
                S_StartMusic(mus_dm2ttl);
            else
                S_StartMusic(mus_intro);
            break;
        case 1:
            G_DeferedPlayDemo(DEH_String("demo1"));
            break;
        case 2:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            pagename = DEH_String("CREDIT");
            break;
        case 3:
            G_DeferedPlayDemo(DEH_String("demo2"));
            break;
        case 4:
            gamestate = GS_DEMOSCREEN;
            if (gamemode == commercial) {
                pagetic = TICRATE * 11;
                pagename = DEH_String("TITLEPIC");
                S_StartMusic(mus_dm2ttl);
            } else {
                pagetic = 200;

                if (gameversion >= exe_ultimate)
                    pagename = DEH_String("CREDIT");
                else
                    pagename = DEH_String("HELP2");
            }
            break;
        case 5:
            G_DeferedPlayDemo(DEH_String("demo3"));
            break;
            // THE DEFINITIVE DOOM Special Edition demo
        case 6:
            G_DeferedPlayDemo(DEH_String("demo4"));
            break;
    }

    // The Doom 3: BFG Edition version of doom2.wad does not have a
    // TITLETPIC lump. Use INTERPIC instead as a workaround.
    if (gamevariant == bfgedition && !strcasecmp(pagename, "TITLEPIC") &&
        W_CheckNumForName("titlepic") < 0) {
        pagename = DEH_String("INTERPIC");
    }
}

//
// D_StartTitle
//
void D_StartTitle(void) {
    printf("D_StartTitle\n");
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo();
}

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static char* banners[] = {
    // doom2.wad
    "                         "
    "DOOM 2: Hell on Earth v%i.%i"
    "                           ",
    // doom2.wad v1.666
    "                         "
    "DOOM 2: Hell on Earth v%i.%i66"
    "                          ",
    // doom1.wad
    "                            "
    "DOOM Shareware Startup v%i.%i"
    "                           ",
    // doom.wad
    "                            "
    "DOOM Registered Startup v%i.%i"
    "                           ",
    // Registered DOOM uses this
    "                          "
    "DOOM System Startup v%i.%i"
    "                          ",
    // Doom v1.666
    "                          "
    "DOOM System Startup v%i.%i66"
    "                          "
    // doom.wad (Ultimate DOOM)
    "                         "
    "The Ultimate DOOM Startup v%i.%i"
    "                        ",
    // tnt.wad
    "                     "
    "DOOM 2: TNT - Evilution v%i.%i"
    "                           ",
    // plutonia.wad
    "                   "
    "DOOM 2: Plutonia Experiment v%i.%i"
    "                           ",
};

//
// Get game name: if the startup banner has been replaced, use that.
// Otherwise, use the name given
//

static char* GetGameName(char* gamename) {
    size_t i;
    char* deh_sub;

    for (i = 0; i < arrlen(banners); ++i) {
        // Has the banner been replaced?

        deh_sub = DEH_String(banners[i]);

        if (deh_sub != banners[i]) {
            size_t gamename_size;
            int version;

            // Has been replaced.
            // We need to expand via printf to include the Doom version number
            // We also need to cut off spaces to get the basic name

            gamename_size = strlen(deh_sub) + 10;
            gamename = Z_Malloc(gamename_size, PU_STATIC, 0);
            version = G_VanillaVersionCode();
            M_snprintf(gamename, gamename_size, deh_sub, version / 100,
                       version % 100);

            while (gamename[0] != '\0' && isspace(gamename[0])) {
                memmove(gamename, gamename + 1, gamename_size - 1);
            }

            while (gamename[0] != '\0' &&
                   isspace(gamename[strlen(gamename) - 1])) {
                gamename[strlen(gamename) - 1] = '\0';
            }

            return gamename;
        }
    }

    return gamename;
}

static void SetMissionForPackName(char* pack_name) {
    int i;
    static const struct {
        char* name;
        int mission;
    } packs[] = {
        {"doom2", doom2},
        {"tnt", pack_tnt},
        {"plutonia", pack_plut},
    };

    for (i = 0; i < arrlen(packs); ++i) {
        if (!strcasecmp(pack_name, packs[i].name)) {
            gamemission = packs[i].mission;
            return;
        }
    }

    printf("Valid mission packs are:\n");

    for (i = 0; i < arrlen(packs); ++i) {
        printf("\t%s\n", packs[i].name);
    }

    I_Error("Unknown mission pack name: %s", pack_name);
}

//
// Find out what version of Doom is playing.
//

void D_IdentifyVersion(void) {
    // gamemission is set up by the D_FindIWAD function.  But if
    // we specify '-iwad', we have to identify using
    // IdentifyIWADByName.  However, if the iwad does not match
    // any known IWAD name, we may have a dilemma.  Try to
    // identify by its contents.

    if (gamemission == none) {
        unsigned int i;

        for (i = 0; i < numlumps; ++i) {
            if (!strncasecmp(W_LumpName(i), "MAP01", 8)) {
                gamemission = doom2;
                break;
            } else if (!strncasecmp(W_LumpName(i), "E1M1", 8)) {
                gamemission = doom;
                break;
            }
        }

        if (gamemission == none) {
            // Still no idea.  I don't think this is going to work.

            I_Error("Unknown or invalid IWAD file.");
        }
    }

    // Make sure gamemode is set up correctly

    if (logical_gamemission == doom) {
        // Doom 1.  But which version?

        if (W_CheckNumForName("E4M1") > 0) {
            // Ultimate Doom

            gamemode = retail;
        } else if (W_CheckNumForName("E3M1") > 0) {
            gamemode = registered;
        } else {
            gamemode = shareware;
        }
    } else {
        int p;

        // Doom 2 of some kind.
        gamemode = commercial;

        // We can manually override the gamemission that we got from the
        // IWAD detection code. This allows us to eg. play Plutonia 2
        // with Freedoom and get the right level names.

        //!
        // @category compat
        // @arg <pack>
        //
        // Explicitly specify a Doom II "mission pack" to run as, instead of
        // detecting it based on the filename. Valid values are: "doom2",
        // "tnt" and "plutonia".
        //
        p = M_CheckParmWithArgs("-pack", 1);
        if (p > 0) {
            SetMissionForPackName(myargv[p + 1]);
        }
    }
}

// Set the gamedescription string

void D_SetGameDescription(void) {
    gamedescription = "Unknown";

    if (logical_gamemission == doom) {
        // Doom 1.  But which version?

        if (gamevariant == freedoom) {
            gamedescription = GetGameName("Freedoom: Phase 1");
        } else if (gamemode == retail) {
            // Ultimate Doom

            gamedescription = GetGameName("The Ultimate DOOM");
        } else if (gamemode == registered) {
            gamedescription = GetGameName("DOOM Registered");
        } else if (gamemode == shareware) {
            gamedescription = GetGameName("DOOM Shareware");
        }
    } else {
        // Doom 2 of some kind.  But which mission?

        if (gamevariant == freedm) {
            gamedescription = GetGameName("FreeDM");
        } else if (gamevariant == freedoom) {
            gamedescription = GetGameName("Freedoom: Phase 2");
        } else if (logical_gamemission == doom2) {
            gamedescription = GetGameName("DOOM 2: Hell on Earth");
        } else if (logical_gamemission == pack_plut) {
            gamedescription = GetGameName("DOOM 2: Plutonia Experiment");
        } else if (logical_gamemission == pack_tnt) {
            gamedescription = GetGameName("DOOM 2: TNT - Evilution");
        }
    }
}

//      print title for every printed line
char title[128];

static boolean D_AddFile(char* filename) {
    wad_file_t* handle;

    printf(" adding %s\n", filename);
    handle = W_AddFile(filename);

    return handle != NULL;
}

// Prints a message only if it has been modified by dehacked.

void PrintDehackedBanners(void) {}

static struct {
    char* description;
    char* cmdline;
    GameVersion_t version;
} gameversions[] = {
    {"Doom 1.666", "1.666", exe_doom_1_666},
    {"Doom 1.7/1.7a", "1.7", exe_doom_1_7},
    {"Doom 1.8", "1.8", exe_doom_1_8},
    {"Doom 1.9", "1.9", exe_doom_1_9},
    {"Hacx", "hacx", exe_hacx},
    {"Ultimate Doom", "ultimate", exe_ultimate},
    {"Final Doom", "final", exe_final},
    {"Final Doom (alt)", "final2", exe_final2},
    {"Chex Quest", "chex", exe_chex},
    {NULL, NULL, 0},
};

// Initialize the game version

static void InitGameVersion(void) {
    byte* demolump;
    char demolumpname[6];
    int demoversion;
    int p;
    int i;
    boolean status;

    //!
    // @arg <version>
    // @category compat
    //
    // Emulate a specific version of Doom.  Valid values are "1.666",
    // "1.7", "1.8", "1.9", "ultimate", "final", "final2", "hacx" and
    // "chex".
    //

    p = M_CheckParmWithArgs("-gameversion", 1);

    if (p) {
        for (i = 0; gameversions[i].description != NULL; ++i) {
            if (!strcmp(myargv[p + 1], gameversions[i].cmdline)) {
                gameversion = gameversions[i].version;
                break;
            }
        }

        if (gameversions[i].description == NULL) {
            printf("Supported game versions:\n");

            for (i = 0; gameversions[i].description != NULL; ++i) {
                printf("\t%s (%s)\n", gameversions[i].cmdline,
                       gameversions[i].description);
            }

            I_Error("Unknown game version '%s'", myargv[p + 1]);
        }
    } else {
        // Determine automatically

        if (gamemission == pack_chex) {
            // chex.exe - identified by iwad filename

            gameversion = exe_chex;
        } else if (gamemission == pack_hacx) {
            // hacx.exe: identified by iwad filename

            gameversion = exe_hacx;
        } else if (gamemode == shareware || gamemode == registered ||
                   (gamemode == commercial && gamemission == doom2)) {
            // original
            gameversion = exe_doom_1_9;
            printf("NRFD-TODO: game detect\n");
        } else if (gamemode == retail) {
            gameversion = exe_ultimate;
        } else if (gamemode == commercial) {
            // Final Doom: tnt or plutonia
            // Defaults to emulating the first Final Doom executable,
            // which has the crash in the demo loop; however, having
            // this as the default should mean that it plays back
            // most demos correctly.

            gameversion = exe_final;
        }
    }

    // The original exe does not support retail - 4th episode not supported

    if (gameversion < exe_ultimate && gamemode == retail) {
        gamemode = registered;
    }

    // EXEs prior to the Final Doom exes do not support Final Doom.

    if (gameversion < exe_final && gamemode == commercial &&
        (gamemission == pack_tnt || gamemission == pack_plut)) {
        gamemission = doom2;
    }
}

void PrintGameVersion(void) {
    int i;
    for (i = 0; gameversions[i].description != NULL; ++i) {
        if (gameversions[i].version == gameversion) {
            printf(
                "Emulating the behavior of the "
                "'%s' executable.\n",
                gameversions[i].description);
            break;
        }
    }
}

// Function called at exit to display the ENDOOM screen

static void D_Endoom(void) {}

// Load dehacked patches needed for certain IWADs.
static void LoadIwadDeh(void) { printf("NRFD-TODO: LoadIwadDeh\n"); }

static void G_CheckDemoStatusAtExit(void) {}

//
// D_DoomMain
//
void D_DoomMain(void) {
    int p;
    char file[256];
    char demolumpname[9];
    int numiwadlumps;

    I_AtExit(D_Endoom, false);

    // print banner

    I_PrintBanner(DOOM_PACKAGE_STRING);

    DEH_printf("Z_Init: Init zone memory allocation daemon. \n");
    Z_Init();

    //!
    // @vanilla
    //
    // Disable monsters.
    //

    nomonsters = M_CheckParm("-nomonsters");

    //!
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    respawnparm = M_CheckParm("-respawn");

    //!
    // @vanilla
    //
    // Monsters move faster.
    //

    fastparm = M_CheckParm("-fast");

    //!
    // @vanilla
    //
    // Developer mode.  F1 saves a screenshot in the current working
    // directory.
    //

    devparm = M_CheckParm("-devparm");

    I_DisplayFPSDots(devparm);

    if (devparm) DEH_printf(D_DEVSTR);

    {
        // Auto-detect the configuration dir.

        M_SetConfigDir(NULL);
    }

    //!
    // @arg <x>
    // @vanilla
    //
    // Turbo mode.  The player's speed is multiplied by x%.  If unspecified,
    // x defaults to 200.  Values are rounded up to 10 and down to 400.
    //

    // init subsystems
    DEH_printf("V_Init: allocate screens.\n");
    V_Init();

    // Load configuration files before initialising other subsystems.
    DEH_printf("M_LoadDefaults: Load system defaults.\n");
    M_SetConfigFilenames("default.cfg", DOOM_PROGRAM_PREFIX "doom.cfg");
    D_BindVariables();
    M_LoadDefaults();

    // Save configuration at exit.
    I_AtExit(M_SaveDefaults, false);

    // Find main IWAD file and load it.
    if (no_sdcard) {
        iwadfile = "doom.wad";
    } else {
        iwadfile = D_FindIWAD(IWAD_MASK_DOOM, &gamemission);
    }
    iwadfile = "/SD:/doom.wad";

    // None found?

    if (iwadfile == NULL) {
        I_Error(
            "Game mode indeterminate.  No IWAD file was found.  Try\n"
            "specifying one with the '-iwad' command line parameter.\n");
    }
    printf("WAD File: %s\n", iwadfile);
    // modifiedgame = false; // NRFD-TODO: modified game / PWAD?

    DEH_printf("W_Init: Init WADfiles.\n");
    D_AddFile(iwadfile);
    numiwadlumps = numlumps;

    printf("W_CheckCorrectIWAD\n");
    W_CheckCorrectIWAD(doom);

    // Now that we've loaded the IWAD, we can figure out what gamemission
    // we're playing and which version of Vanilla Doom we need to emulate.
    printf("D_IdentifyVersion\n");
    D_IdentifyVersion();
    printf("InitGameVersion\n");
    InitGameVersion();

    printf("Game version: \n");
    PrintGameVersion();

    // Check which IWAD variant we are using.

    if (W_CheckNumForName("FREEDOOM") >= 0) {
        if (W_CheckNumForName("FREEDM") >= 0) {
            gamevariant = freedm;
        } else {
            gamevariant = freedoom;
        }
    } else if (W_CheckNumForName("DMENUPIC") >= 0) {
        gamevariant = bfgedition;
    }

    // Generate the WAD hash table.  Speed things up a bit.
    W_GenerateHashTable();

    // Set the gamedescription string. This is only possible now that
    // we've finished loading Dehacked patches.
    printf("D_SetGameDescription\n");
    D_SetGameDescription();

    savegamedir = M_GetSaveGameDir(D_SaveGameIWADName(gamemission));

    I_PrintStartupBanner(gamedescription);
    PrintDehackedBanners();

    // Freedoom's IWADs are Boom-compatible, which means they usually
    // don't work in Vanilla (though FreeDM is okay). Show a warning
    // message and give a link to the website.
    if (gamevariant == freedoom) {
        printf(
            " WARNING: You are playing using one of the Freedoom IWAD\n"
            " files, which might not work in this port. See this page\n"
            " for more information on how to play using Freedoom:\n"
            "   https://www.chocolate-doom.org/wiki/index.php/Freedoom\n");
        I_PrintDivider();
    }

    DEH_printf("I_Init: Setting up machine state.\n");

    I_InitTimer();

    I_InitJoystick();
    I_InitSound(true);
    I_InitMusic();

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    //!
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParmWithArgs("-skill", 1);

    if (p) {
        startskill = myargv[p + 1][0] - '1';
        autostart = true;
    }

    //!
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParmWithArgs("-episode", 1);

    if (p) {
        startepisode = myargv[p + 1][0] - '0';
        startmap = 1;
        autostart = true;
    }

    timelimit = 0;

    //!
    // @arg <n>
    // @category net
    // @vanilla
    //
    // For multiplayer games: exit each level after n minutes.
    //

    p = M_CheckParmWithArgs("-timer", 1);

    if (p) {
        timelimit = atoi(myargv[p + 1]);
    }

    //!
    // @category net
    // @vanilla
    //
    // Austin Virtual Gaming: end levels after 20 minutes.
    //

    p = M_CheckParm("-avg");

    if (p) {
        timelimit = 20;
    }

    //!
    // @arg [<x> <y> | <xy>]
    // @vanilla
    //
    // Start a game immediately, warping to ExMy (Doom 1) or MAPxy
    // (Doom 2)
    //

    p = M_CheckParmWithArgs("-warp", 1);

    if (p) {
        if (gamemode == commercial)
            startmap = atoi(myargv[p + 1]);
        else {
            startepisode = myargv[p + 1][0] - '0';

            if (p + 2 < myargc) {
                startmap = myargv[p + 2][0] - '0';
            } else {
                startmap = 1;
            }
        }
        autostart = true;
    }

    // Undocumented:
    // Invoked by setup to test the controls.
    /* NRFD-EXCLUDE
    p = M_CheckParm("-testcontrols");

    if (p > 0)
    {
        startepisode = 1;
        startmap = 1;
        autostart = true;
        testcontrols = true;
    }
    */

    // Check for load game parameter
    // We do this here and save the slot number, so that the network code
    // can override it or send the load slot to other players.

    //!
    // @arg <s>
    // @vanilla
    //
    // Load the game in slot s.
    //

    p = M_CheckParmWithArgs("-loadgame", 1);

    if (p) {
        startloadgame = atoi(myargv[p + 1]);
    } else {
        // Not loading a game
        startloadgame = -1;
    }

    DEH_printf("M_Init: Init miscellaneous info.\n");
    M_Init();

    DEH_printf("R_Init: Init DOOM refresh daemon\n");
    R_Init();

    DEH_printf("\nP_Init: Init Playloop state.\n");
    P_Init();

    DEH_printf("S_Init: Setting up sound.\n");
    S_Init(sfxVolume * 8, musicVolume * 8);

    DEH_printf("D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame();

    PrintGameVersion();

    DEH_printf("HU_Init: Setting up heads up display.\n");
    HU_Init();

    DEH_printf("ST_Init: Init status bar.\n");
    ST_Init();

    if (gameaction != ga_loadgame) {
        if (autostart || netgame)
            G_InitNew(startskill, startepisode, startmap);
        else
            D_StartTitle();  // start up intro loop
    }

    N_rjoy_init();

    D_DoomLoop();  // never returns
}
