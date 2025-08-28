cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_doom)

target_include_directories(app PRIVATE src/)
target_include_directories(app PRIVATE src/config)

target_sources(app PRIVATE
                src/main.c
                src/n_qspi.c
                src/n_fs.c
                src/n_mem.c
                src/n_buttons.c
                src/n_display.c
                src/n_rjoy.c
                src/n_i2s.c
                src/n_i2s_sound.c
                src/deh_main.c
                src/deh_str.c
                src/doom/deh_misc.h
                src/doom/deh_misc.c
                src/d_event.c
                src/d_iwad.c
                src/d_mode.c
                src/d_loop.c
                src/i_timer.c
                src/i_system.c
                src/i_input.c
                src/i_video.c
                src/i_joystick.c
                src/i_sound.c
                src/m_argv.c
                src/m_bbox.c
                src/m_cheat.c
                src/m_config.c
                src/m_fixed.c
                src/m_misc.c
                src/m_controls.c
                src/net_client.c
                src/net_server.c
                src/tables.c
                src/v_diskicon.c
                src/v_video.c
                src/w_checksum.c
                src/w_file.c
                src/w_file_fatfs.c
                src/w_main.c
                src/w_wad.c
                src/z_native.c
                src/bluetooth_control.c
                src/doom/am_map.c
                src/doom/doomstat.c
                src/doom/d_main.c
                src/doom/d_net.c
                src/doom/d_items.c
                src/doom/dstrings.c
                src/doom/f_wipe.c
                src/doom/f_finale.c
                src/doom/g_game.c
                src/doom/hu_lib.c
                src/doom/hu_stuff.c
                src/doom/info.c
                src/doom/m_menu.c
                src/doom/m_random.c
                src/doom/p_doors.c
                src/doom/p_ceilng.c
                src/doom/p_enemy.c
                src/doom/p_floor.c
                src/doom/p_lights.c
                src/doom/p_plats.c
                src/doom/p_telept.c
                src/doom/p_setup.c
                src/doom/p_sight.c
                src/doom/p_saveg.c
                src/doom/p_map.c
                src/doom/p_maputl.c
                src/doom/p_mobj.c
                src/doom/p_inter.c
                src/doom/p_tick.c
                src/doom/p_spec.c
                src/doom/p_switch.c
                src/doom/p_pspr.c
                src/doom/p_user.c
                src/doom/r_bsp.c
                src/doom/r_main.c
                src/doom/r_data.c
                src/doom/r_draw.c
                src/doom/r_plane.c
                src/doom/r_segs.c
                src/doom/r_sky.c
                src/doom/r_things.c
                src/doom/s_sound.c
                src/doom/st_lib.c
                src/doom/st_stuff.c
                src/doom/statdump.c
                src/doom/sounds.c
                src/doom/wi_stuff.c
                )


# set C version
set(CMAKE_C_STANDARD 89)

# set oldest CPP version
set(CMAKE_CXX_STANDARD  98)
