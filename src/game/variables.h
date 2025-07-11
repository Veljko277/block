/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times

// client
// MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict client movements")
// MACRO_CONFIG_INT(ClAntiPingLimit, cl_antiping_limit, 0, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Antiping limit (0 to disable)")
// MACRO_CONFIG_INT(ClAntiPing, cl_antiping, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable antiping, i. e. more aggressive prediction.")
// MACRO_CONFIG_INT(ClAntiPingPlayers, cl_antiping_players, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict other player's movement more aggressively (only enabled if cl_antiping is set to 1)")
// MACRO_CONFIG_INT(ClAntiPingGrenade, cl_antiping_grenade, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict grenades (only enabled if cl_antiping is set to 1)")
// MACRO_CONFIG_INT(ClAntiPingWeapons, cl_antiping_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict weapon projectiles (only enabled if cl_antiping is set to 1)")
// MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show name plates")
// MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Always show name plates disregarding of distance")
// MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use team colors for name plates")
// MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the name plates from 0 to 100%")
// MACRO_CONFIG_INT(ClNameplatesClan, cl_nameplates_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clan in name plates")
// MACRO_CONFIG_INT(ClNameplatesClanSize, cl_nameplates_clan_size, 30, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the clan plates from 0 to 100%")
// MACRO_CONFIG_INT(ClTextEntities, cl_text_entities, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render textual entity data")
// #if defined(__ANDROID__)
// MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto switch weapon on pickup")
// MACRO_CONFIG_INT(ClAutoswitchWeaponsOutOfAmmo, cl_autoswitch_weapons_out_of_ammo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto switch weapon when out of ammo")
// #else
// MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto switch weapon on pickup")
// MACRO_CONFIG_INT(ClAutoswitchWeaponsOutOfAmmo, cl_autoswitch_weapons_out_of_ammo, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto switch weapon when out of ammo")
// #endif

// MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD")
// MACRO_CONFIG_INT(ClShowhudHealthAmmo, cl_showhud_healthammo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Health + Ammo)")
// MACRO_CONFIG_INT(ClShowhudScore, cl_showhud_score, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Score)")
// MACRO_CONFIG_INT(ClShowRecord, cl_showrecord, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show old style DDRace client records")
// MACRO_CONFIG_INT(ClShowChat, cl_showchat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat")
// MACRO_CONFIG_INT(ClShowChatFriends, cl_show_chat_friends, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show only chat messages from friends")
// MACRO_CONFIG_INT(ClShowKillMessages, cl_showkillmessages, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show kill messages")
// MACRO_CONFIG_INT(ClShowVotesAfterVoting, cl_show_votes_after_voting, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show votes window after voting")
// MACRO_CONFIG_INT(ClShowLocalTimeAlways, cl_show_local_time_always, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Always show local time")
// MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame FPS counter")
// MACRO_CONFIG_INT(ClShowpred, cl_showpred, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame prediction time in milliseconds")
// MACRO_CONFIG_INT(ClEyeWheel, cl_eye_wheel, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show eye wheel along together with emotes")
// MACRO_CONFIG_INT(ClEyeDuration, cl_eye_duration, 999999, 1, 999999, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long the eyes emotes last")

// MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
// MACRO_CONFIG_INT(ClThreadsoundloading, cl_threadsoundloading, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Load sound files threaded")

// MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Warn about team balance")

// #if defined(__ANDROID__)
// MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 800, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "") // Disable dynamic camera on Android, screen becomes jerky when you tap joystick
// #else
// MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
// #endif
// MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
// #if defined(__ANDROID__)
// MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 400, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "") // Prevent crosshair from moving out of screen on Android
// #else
// MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 800, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
// #endif

// MACRO_CONFIG_INT(ClDyncam, cl_dyncam, 0, 0, 1, CFGFLAG_CLIENT, "Enable dyncam")
// MACRO_CONFIG_INT(ClDyncamMaxDistance, cl_dyncam_max_distance, 1000, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximal dynamic camera distance")
// MACRO_CONFIG_INT(ClDyncamMousesens, cl_dyncam_mousesens, 0, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse sens used when dyncam is toggled on")
// MACRO_CONFIG_INT(ClDyncamDeadzone, cl_dyncam_deadzone, 300, 1, 1300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera dead zone")
// MACRO_CONFIG_INT(ClDyncamFollowFactor, cl_dyncam_follow_factor, 60, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera follow factor")

// MACRO_CONFIG_INT(EdZoomTarget, ed_zoom_target, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Zoom to the current mouse target")
// MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClBlockExp, cl_block_exp, 3, 1, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

//MACRO_CONFIG_INT(ClFlow, cl_flow, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

// MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
// MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long to show the server message of the day")

// MACRO_CONFIG_STR(ClDDNetVersionServer, cl_ddnet_version_server, 100, "version.ddnet.tw", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server to use to check for new ddnet versions")
// MACRO_CONFIG_STR(ClDDNetUpdate2Server, cl_ddnet_update2_server, 100, "update2.ddnet.tw", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server to use to update new ddnet versions")
// MACRO_CONFIG_STR(ClDDNetMapServer, cl_ddnet_maps_server, 100, "maps.ddnet.tw", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server to use to download maps")

// MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What language file to use")
// MACRO_CONFIG_INT(ClVanillaSkinsOnly, cl_vanilla_skins_only, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show skins available in Vanilla Teeworlds")
// MACRO_CONFIG_INT(ClAutoStatboardScreenshot, cl_auto_statboard_screenshot, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically take game over statboard screenshot")
// MACRO_CONFIG_INT(ClAutoStatboardScreenshotMax, cl_auto_statboard_screenshot_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically created statboard screenshots (0 = no limit)")

// MACRO_CONFIG_INT(ClDefaultZoom, cl_default_zoom, 10, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Default zoom level (10 default, min 0, max 20)")

// MACRO_CONFIG_INT(ClPlayerUseCustomColor, player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles usage of custom colors")
// MACRO_CONFIG_INT(ClPlayerColorBody, player_color_body, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Player body color")
// MACRO_CONFIG_INT(ClPlayerColorFeet, player_color_feet, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Player feet color")
// MACRO_CONFIG_STR(ClPlayerSkin, player_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Player skin")

// MACRO_CONFIG_INT(UiPage, ui_page, 6, 0, 11, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface page")
// MACRO_CONFIG_INT(UiToolboxPage, ui_toolbox_page, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toolbox page")
// MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface server address")
// MACRO_CONFIG_INT(UiScale, ui_scale, 100, 50, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface scale")
// MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 100, 5, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")

// MACRO_CONFIG_INT(UiColorHue, ui_color_hue, 160, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface color hue")
// MACRO_CONFIG_INT(UiColorSat, ui_color_sat, 70, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface color saturation")
// MACRO_CONFIG_INT(UiColorLht, ui_color_lht, 175, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface color lightness")
// MACRO_CONFIG_INT(UiColorAlpha, ui_color_alpha, 228, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface alpha")

// MACRO_CONFIG_INT(UiColorizePing, ui_colorize_ping, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Highlight ping")
// MACRO_CONFIG_INT(UiColorizeGametype, ui_colorize_gametype, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Highlight gametype")

// MACRO_CONFIG_STR(UiDemoSelected, ui_demo_selected, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Selected demo file")

// MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable clipping")

// limitations
MACRO_CONFIG_INT(SvDeathNoteCoolDown, sv_deathnote_cooldown, 60, 1, 1800, CFGFLAG_SERVER, "time in seconds the player must wait before using the deathnote cmd again")

// blocking
MACRO_CONFIG_INT(SvBlockTime, sv_block_time, 3, 3, 10, CFGFLAG_SERVER, "time freezed for a player to be marked blocked")
MACRO_CONFIG_INT(SvQuestCount, sv_player_quest_count, 8, 3, 64, CFGFLAG_SERVER, "amount of playrs needed to begin quests")
MACRO_CONFIG_INT(SvLvlCount, sv_player_lvlsystem_count, 8, 2, 64, CFGFLAG_SERVER, "amount of players needed for the lvl system to enable")
MACRO_CONFIG_INT(SvAntiFarmDuration, sv_antifarm_block_dur, 15, 5, 500, CFGFLAG_SERVER, "how long a player has to been living in seconds to get blocked for points")
MACRO_CONFIG_INT(SvSaberKill, sv_saber_kill, 0, 0, 1, CFGFLAG_SERVER, "enable to kill with saber")

// account
MACRO_CONFIG_INT(SvAccountSlots, sv_account_slots, 3, 1, 64, CFGFLAG_SERVER, "How many players can be conected to the same account between other servers")

// dummy
// MACRO_CONFIG_STR(ClDummyName, dummy_name, 16, "brainless tee", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Name of the Dummy")
// MACRO_CONFIG_STR(ClDummyClan, dummy_clan, 12, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Clan of the Dummy")
// MACRO_CONFIG_INT(ClDummyCountry, dummy_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Country of the Dummy")
// MACRO_CONFIG_INT(ClDummyUseCustomColor, dummy_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles usage of custom colors")
// MACRO_CONFIG_INT(ClDummyColorBody, dummy_color_body, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dummy body color")
// MACRO_CONFIG_INT(ClDummyColorFeet, dummy_color_feet, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dummy feet color")
// MACRO_CONFIG_STR(ClDummySkin, dummy_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dummy skin")
MACRO_CONFIG_INT(ClDummy, cl_dummy, 0, 0, 1, CFGFLAG_CLIENT, "0 - player / 1 - dummy")
// MACRO_CONFIG_INT(ClDummyHammer, cl_dummy_hammer, 0, 0, 1, CFGFLAG_CLIENT, "Whether dummy is hammering for a hammerfly")
// MACRO_CONFIG_INT(ClDummyResetOnSwitch, cl_dummy_resetonswitch, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether dummy should stop pressing keys when you switch")
// MACRO_CONFIG_INT(ClDummyCopyMoves, cl_dummy_copy_moves, 0, 0, 1, CFGFLAG_CLIENT, "Wether dummy should copy your moves")

// curl http download
// MACRO_CONFIG_INT(ClHTTPConnectTimeoutMs, cl_http_connect_timeout_ms, 2000, 0, 100000, CFGFLAG_CLIENT, "HTTP downloads: timeout for the connect phase in milliseconds (0 to disable)")
// MACRO_CONFIG_INT(ClHTTPLowSpeedLimit, cl_http_low_speed_limit, 500, 0, 100000, CFGFLAG_CLIENT, "HTTP downloads: Set low speed limit in bytes per second (0 to disable)")
// MACRO_CONFIG_INT(ClHTTPLowSpeedTime, cl_http_low_speed_time, 5, 0, 100000, CFGFLAG_CLIENT, "HTTP downloads: Set low speed limit time period (0 to disable)")

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTeamdamage, sv_teamdamage, 0, 0, 1, CFGFLAG_SERVER, "Team damage")
MACRO_CONFIG_STR(SvMaprotation, sv_maprotation, 768, "", CFGFLAG_SERVER, "Maps to rotate between")
MACRO_CONFIG_INT(SvRoundsPerMap, sv_rounds_per_map, 1, 1, 100, CFGFLAG_SERVER, "Number of rounds on each map before rotating")
MACRO_CONFIG_INT(SvRoundSwap, sv_round_swap, 1, 0, 1, CFGFLAG_SERVER, "Swap teams between rounds")
MACRO_CONFIG_INT(SvPowerups, sv_powerups, 1, 0, 1, CFGFLAG_SERVER, "Allow powerups like ninja")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 20, 0, 1000, CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 1, CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvRespawnDelayTDM, sv_respawn_delay_tdm, 3, 0, 10, CFGFLAG_SERVER, "Time needed to respawn after death in tdm gametype")

MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvTeambalanceTime, sv_teambalance_time, 1, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before autobalancing teams")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 0, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 0, 0, 2, CFGFLAG_SERVER, "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time in seconds to ban a player if kicked by vote. 0 makes it just use kick")
MACRO_CONFIG_INT(SvJoinVoteDelay, sv_join_vote_delay, 60, 0, 1000, CFGFLAG_SERVER, "Add a delay before recently joined players can vote (in seconds)")
MACRO_CONFIG_INT(SvOldTeleportWeapons, sv_old_teleport_weapons, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Teleporting of all weapons (deprecated, use special entities instead)");
MACRO_CONFIG_INT(SvOldTeleportHook, sv_old_teleport_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Hook through teleporter (deprecated, use special entities instead)");
MACRO_CONFIG_INT(SvTeleportHoldHook, sv_teleport_hold_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Hold hook when teleported");
MACRO_CONFIG_INT(SvTeleportLoseWeapons, sv_teleport_lose_weapons, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Lose weapons when teleported (useful for some race maps)");
MACRO_CONFIG_INT(SvDeepfly, sv_deepfly, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Allow fire non auto weapons when deep");

MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 5, 1, 100, CFGFLAG_SERVER, "64 player id <-> vanilla id players map update rate")

MACRO_CONFIG_INT(SvSkinStealAction, sv_skinstealaction, 0, 0, 1, CFGFLAG_SERVER, "How to punish skin stealing (currently only 1 = force pinky)")
MACRO_CONFIG_STR(SvServerType, sv_server_type, 64, "none", CFGFLAG_SERVER, "Type of the server (novice, moderate, ...)")

MACRO_CONFIG_INT(SvSendVotesPerTick, sv_send_votes_per_tick, 5, 1, 15, CFGFLAG_SERVER, "Number of vote options being send per tick")

MACRO_CONFIG_INT(SvWbProt, sv_wbprot, 2, 0, 2, CFGFLAG_SERVER, "Wayblock protection, 1 - everyone, 2 - members only")
MACRO_CONFIG_INT(SvQuestRaceTime, sv_quest_race_time, 0, 0, 60, CFGFLAG_SERVER | CFGFLAG_GAME, "[map config] Minutes a player has to complete the race quest")
MACRO_CONFIG_INT(SvBlackHoleExpiretime, sv_blackhole_expire_time, 20, 5, 1000, CFGFLAG_SERVER, "amount of time in seconds a blackhole expires")
MACRO_CONFIG_INT(SvBlackHolescount, sv_blackholes_count, 3, 1, 10, CFGFLAG_SERVER, "amount of blackholes that can be set by one player")
MACRO_CONFIG_INT(SvBlackHoleKills, sv_blackhole_kills, 1, 0, 1, CFGFLAG_SERVER, "black hole can kill?")
MACRO_CONFIG_INT(SvKOHCircleRadius, sv_koh_circle_radius, 400, 0, 0x7FFFFFFF, CFGFLAG_SERVER, "Radius for king of the hill zone")
MACRO_CONFIG_INT(SvKOHCircleType, sv_koh_circle_type, 0, 0, 5, CFGFLAG_SERVER, "Type of entity the circle will use")
MACRO_CONFIG_INT(SvKOHCaptureXpLimit, sv_koh_capture_xp_limit, 700, 100, 10000, CFGFLAG_SERVER, "Required number of zone xp to get a point")
MACRO_CONFIG_INT(SvKOHRequiredPoints, sv_koh_required_points, 3, 1, 10, CFGFLAG_SERVER, "Required number of points to win the koh")
MACRO_CONFIG_INT(SvRescue, sv_rescue, 0, 0, 1, CFGFLAG_SERVER, "Allow /rescue command so players can teleport themselves out of freeze")
MACRO_CONFIG_INT(SvRescueDelay, sv_rescue_delay, 5, 0, 1000, CFGFLAG_SERVER, "Number of seconds inbetween two rescues")
MACRO_CONFIG_INT(SvLMBRegDuration, sv_lmb_reg_duration, 5, 1, 60, CFGFLAG_SERVER, "Specify the duration where LMB registration is possible")
MACRO_CONFIG_INT(SvLMBFreezeTime, sv_lmb_freeze_time, 15, 5, 30, CFGFLAG_SERVER, "Specifies the time a player needs to be frozen to die")
MACRO_CONFIG_INT(SvLMBTournamentTime, sv_lmb_tournament_time, 5, 1, 20, CFGFLAG_SERVER, "Specifies the maximum time a tournament runs.")
MACRO_CONFIG_INT(SvLMBMinPlayer, sv_lmb_min_player, 2, 2, 64, CFGFLAG_SERVER, "Specifies the minimum players required to start")
MACRO_CONFIG_INT(SvLMBMaxPlayer, sv_lmb_max_player, 64, 2, 64, CFGFLAG_SERVER, "Specifies the maximum playercount")
MACRO_CONFIG_INT(SvLMBSpawnFreezeTime, sv_lmb_spawn_freeze_time, 3, 0, 5, CFGFLAG_SERVER, "Specifies the amount of time a player is frozen at the spawn")
MACRO_CONFIG_INT(SvLMBCooldown, sv_lmb_cooldown, 30, 0, 120, CFGFLAG_SERVER, "Minutes that have to pass unless a new round can start.")
MACRO_CONFIG_INT(SvFlagHuntCooldown, sv_flag_hunt_cooldown, 30, 0, 120, CFGFLAG_SERVER, "Minutes that have to pass unless a new round can start.")
MACRO_CONFIG_INT(SvTrollShake, sv_troll_shake, 1000, 100, 10000, CFGFLAG_SERVER, "Value of troll. Bigger = more shaking.")
MACRO_CONFIG_INT(ClBloodyDelay, cl_bloody_delay, 4, 1, 20, CFGFLAG_SERVER, "make bloody faster or lower")
MACRO_CONFIG_INT(ClSteamyDelay, cl_steamy_delay, 4, 1, 20, CFGFLAG_SERVER, "make steamy faster or lower")
MACRO_CONFIG_INT(ClSaberLenght, cl_saber_lenght, 185, 1, 15000, CFGFLAG_SERVER, "size of the light saber")
MACRO_CONFIG_INT(ClStarsAcc, cl_stars_acc, 1, 1, 360, CFGFLAG_SERVER, "")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, 15, CFGFLAG_SERVER, "")
#endif

// MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
// MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")
#endif
