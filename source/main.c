#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include <tremor/ivorbisfile.h>

#include <3ds.h>
#include "app_wifi_guard.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include "shield_ui_language.h"

/* Translate console UI formats while leaving file names and track metadata
 * untouched. Dynamic UI labels are translated explicitly at their call site. */
#define printf shield_ui_printf

#define MAX_TRACKS 512
#define PATH_CAP 512
#define INPUT_CAP 16384
#define MP3_BAD_DATA_LIMIT 8192
// Larger linear buffers give the Old 3DS CPU more time to decode MP3 data
// before NDSP consumes the queued audio and would otherwise underrun.
#define BLOCK_FRAMES 8192
#define AUDIO_BLOCK_COUNT 4
#define CHANNEL 0
#define TRACK_FADE_MS 150
#define REVERB_BUFFER_SIZE 2048
#define REVERB_AP_BUFFER_SIZE 1024
#define BOTTOM_START_ROW 24
#define BOTTOM_REVERB_LABEL_ROW 19
#define BOTTOM_REVERB_MARKER_ROW 20
#define BOTTOM_REVERB_ROW 21
#define BOTTOM_PROGRESS_LABEL_ROW 26
#define BOTTOM_PROGRESS_ROW 27
#define BOTTOM_TEMP_ROW 29

typedef struct { char name[256]; } Track;
typedef struct { char name[256]; bool directory; } BrowseEntry;
typedef struct { ndspWaveBuf wave; int16_t *pcm; } AudioBlock;

static Track tracks[MAX_TRACKS];
static int track_count, order[MAX_TRACKS], order_pos = -1;
static BrowseEntry entries[MAX_TRACKS];
static int entry_count, selection;
static char folder[PATH_CAP] = "sdmc:/";
static char current_title[256] = "Aucun morceau";
static FILE *audio_file;
static mp3dec_t decoder;
static OggVorbis_File ogg_decoder;
static bool ogg_open, ogg_track;
static int ogg_channels;
static uint8_t input[INPUT_CAP];
static int input_size;
static AudioBlock blocks[AUDIO_BLOCK_COUNT];
static PrintConsole top_console, bottom_console;
static int16_t decoded[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int16_t pending_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
static int16_t ogg_pcm[BLOCK_FRAMES * 2];
static int pending_frames, pending_pos;
static u64 playback_position_ms, playback_last_update_ms, track_duration_ms;
static u64 playback_rate_remainder;
static u64 audio_payload_bytes;
static u64 mp3_data_offset, mp3_xing_bytes;
static u8 mp3_xing_toc[100];
static bool mp3_toc_valid;
static u64 last_touch_seek_ms;
static u64 last_touch_preview_ms, touch_seek_target_ms;
static bool touch_seek_dragging;
static bool touch_reverb_dragging;
static unsigned touch_reverb_preview;
static bool options_open;
static unsigned options_page, options_selection;
static int brightness_level = 3;
static u8 backlight_config[2] = {0, 3};
static bool cfgu_service_ready, backlight_config_valid;
static bool lcd_ready, brightness_changed;
static bool force_language_selection;
static AppWifiGuard wifi_guard;

static void show_boot_splash(void) {
    // Keep the two splash images in the CIA RomFS; no SD access is needed.
    const bool romfs_ready = R_SUCCEEDED(romfsInit());
    const struct {
        gfxScreen_t screen;
        const char *path;
        size_t bytes;
    } images[] = {
        {GFX_TOP, "romfs:/boot-top.bgr", 400 * 240 * 3},
        {GFX_BOTTOM, "romfs:/boot-bottom.bgr", 320 * 240 * 3},
    };
    for (unsigned i = 0; i < sizeof(images) / sizeof(images[0]); ++i) {
        u8 *frame = gfxGetFramebuffer(images[i].screen, GFX_LEFT, NULL, NULL);
        if (!frame) continue;
        memset(frame, 0, images[i].bytes);
        if (romfs_ready) {
            FILE *file = fopen(images[i].path, "rb");
            if (file) {
                if (fread(frame, 1, images[i].bytes, file) != images[i].bytes)
                    memset(frame, 0, images[i].bytes);
                fclose(file);
            }
        }
    }
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
    svcSleepThread(4000000000LL);
    if (romfs_ready) romfsExit();
}

static bool open_guide_save(FS_Archive *archive) {
    FS_Path root = fsMakePath(PATH_EMPTY, "");
    if (R_SUCCEEDED(FSUSER_OpenArchive(archive, ARCHIVE_SAVEDATA, root)))
        return true;
    // Fresh installations have no formatted save archive yet.
    if (R_FAILED(FSUSER_FormatSaveData(ARCHIVE_SAVEDATA, root,
                                       1024, 8, 8, 8, 8, false)))
        return false;
    return R_SUCCEEDED(FSUSER_OpenArchive(archive, ARCHIVE_SAVEDATA, root));
}

static bool guide_completed(FS_Archive archive) {
    Handle file;
    FS_Path path = fsMakePath(PATH_ASCII, "/guide.dat");
    if (R_FAILED(FSUSER_OpenFile(&file, archive, path, FS_OPEN_READ, 0)))
        return false;
    char value = 0;
    u32 read = 0;
    Result result = FSFILE_Read(file, &read, 0, &value, 1);
    FSFILE_Close(file);
    return R_SUCCEEDED(result) && read == 1 && value == '3';
}

static bool save_guide_completed(FS_Archive archive) {
    FS_Path path = fsMakePath(PATH_ASCII, "/guide.dat");
    (void)FSUSER_CreateFile(archive, path, 0, 1);
    Handle file;
    if (R_FAILED(FSUSER_OpenFile(&file, archive, path, FS_OPEN_WRITE, 0))) return false;
    const char value = '3';
    u32 written = 0;
    Result result = FSFILE_Write(file, &written, 0, &value, 1, FS_WRITE_FLUSH);
    FSFILE_Close(file);
    return R_SUCCEEDED(result) && written == 1 &&
           R_SUCCEEDED(FSUSER_ControlArchive(archive,
               ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0));
}

static bool guide_sd_marker(bool write_marker) {
    const char *path = "sdmc:/3ds/ShieldDJLite/guide-v3.dat";
    if (write_marker) {
        (void)mkdir("sdmc:/3ds", 0777);
        (void)mkdir("sdmc:/3ds/ShieldDJLite", 0777);
    }
    FILE *file = fopen(path, write_marker ? "wb" : "rb");
    if (!file) return false;
    char marker = '3';
    bool okay = write_marker ? fwrite(&marker, 1, 1, file) == 1 :
                                fread(&marker, 1, 1, file) == 1 && marker == '3';
    if (fclose(file) != 0) okay = false;
    return okay;
}

static void guide_control(const char *keys, const char *action) {
    printf("   " CONSOLE_CYAN "%-13s" CONSOLE_RESET "%s\n", keys,
           shield_ui_translate(action));
}

static void draw_guide_page(unsigned page) {
    consoleSelect(&top_console);
    consoleClear();
    printf("\n   " CONSOLE_CYAN "SHIELD DJ LITE" CONSOLE_RESET "                         %u/4\n", page + 1);
    printf("   -------------------------------------------\n\n");
    if (page == 0) {
        printf("   " CONSOLE_YELLOW "TA MUSIQUE" CONSOLE_RESET "\n\n");
        printf("   Mets tes MP3 ou OGG/Vorbis dans un dossier\n");
        printf("   de la carte SD.\n");
        printf("   MP3 et OGG/Vorbis seulement.\n");
        printf("   Lite est legere ; Standard est plus soignee.\n\n");
        guide_control("HAUT / BAS", "Choisir un fichier");
        printf("\n");
        guide_control("A", "Ouvrir ou lire");
        printf("\n");
        guide_control("B", "Dossier precedent");
        printf("\n");
        guide_control("Y", "Lire le dossier en aleatoire");
    } else if (page == 1) {
        printf("   " CONSOLE_YELLOW "PENDANT LA LECTURE" CONSOLE_RESET "\n\n\n");
        guide_control("A", "Pause / reprendre");
        printf("\n");
        guide_control("B", "Arreter");
        printf("\n");
        guide_control("L / R", "Morceau precedent / suivant");
        printf("\n");
        guide_control("GAUCHE/DROITE", "Pitch et vitesse");
        printf("\n");
        guide_control("HAUT / BAS", "Luminosite");
        printf(shield_ui_translate("\n   Circle Pad gauche : tour droite bloque L/R.\n"));
        printf(shield_ui_translate("   Si besoin, fais un 2e tour; geste en dev.\n"));
        printf(shield_ui_translate("   Tour gauche : debloque; recentre le pad.\n"));
    } else if (page == 2) {
        printf("   " CONSOLE_YELLOW "REGLAGES ET AIDE" CONSOLE_RESET "\n\n\n");
        guide_control("X", "Repeter le morceau");
        printf("\n");
        guide_control("Y", "Changer les basses");
        printf("\n");
        guide_control("SELECT", "Ouvrir / fermer les options");
        printf("\n");
        guide_control("HOME", "Boost OFF / 2x / 3x / 5x");
        printf("\n");
        guide_control("START", "Quitter l'application");
        printf(shield_ui_language.french ?
               "\n   Filtres : Aucun, Rock, Pop, Jazz,\n"
               "   Clair, Exterieur, Classique.\n"
               "   Effets : reverb libre ou 4 profils.\n"
               "   Reverb prolongee : Options > Effets.\n"
               "   Barre : profils 1-4 ou intensite 0-100%%.\n"
               "   SELECT au boot : revoir ce guide.\n" :
               "\n   Filters: None, Rock, Pop, Jazz,\n"
               "   Clear, Outdoor, Classic.\n"
               "   Effects: free reverb or 4 presets.\n"
               "   Long reverb: Options > Effects.\n"
               "   Bar: presets 1-4 or intensity 0-100%%.\n"
               "   Hold SELECT at boot to replay this guide.\n");
    } else {
        printf("   " CONSOLE_YELLOW "UN PROJET BENEVOLE" CONSOLE_RESET "\n\n\n");
        printf("   Je developpe Shield DJ Lite\n");
        printf("   gratuitement, sur mon temps libre,\n");
        printf("   avec des moyens limites.\n\n");
        printf("   Je fais de mon mieux pour rendre\n");
        printf("   l'application stable et agreable.\n\n");
        printf("   Des bugs restent possibles :\n");
        printf("   je m'en excuse et continuerai\n");
        printf("   a les corriger. Merci !\n");
    }
    top_console.cursorX = 3;
    top_console.cursorY = top_console.windowHeight - 3;
    printf("-------------------------------------------");
    top_console.cursorX = 3;
    top_console.cursorY = top_console.windowHeight - 2;
    if (page == 0)
        printf(CONSOLE_GREEN "A  PAGE SUIVANTE" CONSOLE_RESET);
    else if (page == 3)
        printf("B  RETOUR       " CONSOLE_GREEN "A  J'AI COMPRIS" CONSOLE_RESET);
    else
        printf("B  RETOUR       " CONSOLE_GREEN "A  PAGE SUIVANTE" CONSOLE_RESET);
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

static void show_first_launch_guide(bool force_replay) {
    FS_Archive archive;
    bool save_ready = open_guide_save(&archive);
    hidScanInput();
    bool replay_requested = (hidKeysHeld() & KEY_SELECT) != 0;
    if (!force_replay && !replay_requested &&
        ((save_ready && guide_completed(archive)) || guide_sd_marker(false))) {
        if (save_ready) FSUSER_CloseArchive(archive);
        return;
    }
    consoleSelect(&bottom_console);
    consoleClear();
    unsigned page = 0;
    draw_guide_page(page);
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_A) {
            if (page == 3) {
                if (!save_ready || !save_guide_completed(archive))
                    (void)guide_sd_marker(true);
                break;
            }
            page++;
            draw_guide_page(page);
        } else if ((down & KEY_B) && page > 0) {
            page--;
            draw_guide_page(page);
        }
        svcSleepThread(16000000LL);
    }
    if (save_ready) FSUSER_CloseArchive(archive);
}

static void draw_language_prompt(void) {
    consoleSelect(&top_console);
    consoleClear();
    if (force_language_selection) {
        printf(CONSOLE_CYAN "CHOIX DE LANGUE / LANGUAGE" CONSOLE_RESET "\n\n");
        printf(shield_ui_language.french ? "Choisis la langue de l'application.\n\n" :
                                           "Choose the app language.\n\n");
        printf(CONSOLE_GREEN "A : Francais" CONSOLE_RESET "\n\n");
        printf(CONSOLE_GREEN "B : English" CONSOLE_RESET "\n\n");
    } else if (shield_ui_language.french) {
        printf(CONSOLE_CYAN "LANGUE DETECTEE : FRANCAIS" CONSOLE_RESET "\n\n");
        printf("Seules deux langues sont disponibles :\nFrancais et English.\n\n");
        printf(CONSOLE_GREEN "A : Oui, continuer" CONSOLE_RESET "\n\n");
        printf("B : Non, choisir English\n\n");
        printf("Mon anglais n'est pas parfait; des\nerreurs de traduction sont possibles.\n");
    } else {
        printf(CONSOLE_CYAN "DETECTED LANGUAGE: ENGLISH" CONSOLE_RESET "\n\n");
        printf("Only two languages are available:\nEnglish and French.\n\n");
        printf(CONSOLE_GREEN "A: Yes, continue" CONSOLE_RESET "\n\n");
        printf("B: No, choose French\n\n");
        printf("My English is not perfect; some\ntranslation mistakes may remain.\n");
    }
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

static void choose_ui_language(bool force_choice) {
    shield_ui_language_load(&shield_ui_language,
                            "sdmc:/3ds/ShieldDJLite/ui-language-v1.dat",
                            cfgu_service_ready);
    if (!force_choice && !shield_ui_language.confirmation_needed) return;
    force_language_selection = force_choice;
    draw_language_prompt();
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_A) {
            if (force_choice) shield_ui_language_set(&shield_ui_language, true);
            else shield_ui_language_choose(&shield_ui_language, true);
            break;
        }
        if (down & KEY_B) {
            if (force_choice) shield_ui_language_set(&shield_ui_language, false);
            else shield_ui_language_choose(&shield_ui_language, false);
            break;
        }
        if (down & KEY_START) {
            if (shield_ui_language.confirmation_needed)
                shield_ui_language_choose(&shield_ui_language, true);
            break;
        }
        svcSleepThread(16000000LL);
    }
    force_language_selection = false;
    consoleSelect(&top_console);
    consoleClear();
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

static void copy_name(char *destination, size_t capacity, const char *source) {
    size_t length = strnlen(source, capacity - 1);
    memcpy(destination, source, length);
    destination[length] = 0;
}

// Print a filename/path on one console row, preserving UTF-8 boundaries.
static void print_short(const char *text, size_t max_chars) {
    size_t bytes = strlen(text), pos = 0, chars = 0;
    while (pos < bytes && chars < max_chars) {
        unsigned char c = (unsigned char)text[pos];
        size_t width = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 :
                       (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        if (pos + width > bytes) width = 1;
        pos += width;
        chars++;
    }
    if (pos < bytes && max_chars >= 3) {
        // Reserve room for the ellipsis.
        pos = 0;
        chars = 0;
        while (pos < bytes && chars < max_chars - 3) {
            unsigned char c = (unsigned char)text[pos];
            size_t width = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 :
                           (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
            if (pos + width > bytes) width = 1;
            pos += width;
            chars++;
        }
        fwrite(text, 1, pos, stdout);
        fputs("...", stdout);
    } else {
        fwrite(text, 1, pos, stdout);
    }
}
static bool playing, paused, repeat_current, track_keys_locked;
static bool shuffle = true;
static bool audio_ready, ui_dirty = true, time_dirty, pitch_dirty;
static int playback_time_row, pitch_value_row;
static char clock_text[6] = "--:--";
static u64 clock_next_update_ms;
static bool battery_service_ready;
static bool ptmu_service_ready, charging_state_known, battery_charging;
static float battery_percent = -1.0f;
static u64 battery_last_update;
static u64 charger_last_update;
static bool fs_service_ready, sd_storage_ready;
static u64 sd_total_bytes, sd_free_bytes;
static int battery_temperature_c = -1;
static u64 temperature_last_update;
static int thermal_warning_threshold = 45;
static bool audio_eof;
static int fade_in_remaining = -1, fade_in_total;
static AudioBlock *last_queued_block;
static bool track_has_audio;
static int failed_tracks_in_a_row;
static const char *audio_status = "";
static const char *playback_status = "";
static float speed = 1.0f;
static int pitch_milli = 1000;
static unsigned audio_boost_mode;
static int32_t boost_gain_q15 = 32768;
static u64 last_home_toggle_ms;
static int base_rate = 44100;
static int applied_output_rate, target_output_rate;

static void update_output_rate(void) {
    target_output_rate = (base_rate * pitch_milli + 500) / 1000;
    if (!applied_output_rate) {
        ndspChnSetRate(CHANNEL, (float)target_output_rate);
        applied_output_rate = target_output_rate;
    }
}

static void advance_output_rate(void) {
    if (!target_output_rate || !applied_output_rate ||
        target_output_rate == applied_output_rate) return;
    // Smooth pitch changes without doing any resampling on the CPU.
    int step = base_rate / 200;
    if (step < 40) step = 40;
    int difference = target_output_rate - applied_output_rate;
    if (difference > step) difference = step;
    if (difference < -step) difference = -step;
    applied_output_rate += difference;
    ndspChnSetRate(CHANNEL, (float)applied_output_rate);
}
static unsigned bass_profile;
/* 0=off, then Concert, Cathedral, Extra. */
static unsigned reverb_mode;
static bool reverb_long_enabled;
static const char *const reverb_names[] = { "NON", "CONCERT", "CATHEDRALE", "EXTRA" };
static bool reverb_custom_enabled;
static unsigned reverb_custom_percent = 50;
static int32_t reverb_wet_current_q15;
static int32_t reverb_dry_current_q15 = 32768;
static unsigned filter_profile;
static const char *const filter_names[] = { "Aucun", "Rock", "Pop", "Jazz", "Clair", "Exterieur", "Classique" };
static int32_t filter_low_state[2], filter_high_state[2];
static int32_t filter_gain_current[3] = {32768, 32768, 32768};
static bool filter_state_ready;
static int32_t reverb_comb[2][4][REVERB_BUFFER_SIZE];
static int32_t reverb_damping[2][4];
static int32_t reverb_allpass[2][3][REVERB_AP_BUFFER_SIZE];
static int32_t reverb_smoothed[2];
static unsigned reverb_position;
static const unsigned reverb_comb_delay[2][4] = {
    { 1423, 1493, 1601, 1699 }, { 1451, 1523, 1637, 1747 }
};
static const unsigned reverb_allpass_delay[2][3] = {
    { 347, 113, 251 }, { 353, 127, 263 }
};

static u64 get_payload_bytes(FILE *file, u64 *data_offset) {
    *data_offset = 0;
    struct stat st;
    if (fstat(fileno(file), &st) != 0 || st.st_size <= 0) return 0;
    u64 size = (u64)st.st_size;
    unsigned char id3[10];
    long saved = ftell(file);
    if (saved < 0 || fseek(file, 0, SEEK_SET) != 0) return size;
    if (fread(id3, 1, sizeof(id3), file) == sizeof(id3) &&
        memcmp(id3, "ID3", 3) == 0) {
        u32 tag_size = ((u32)(id3[6] & 0x7f) << 21) |
                       ((u32)(id3[7] & 0x7f) << 14) |
                       ((u32)(id3[8] & 0x7f) << 7) | (id3[9] & 0x7f);
        u64 skip = 10ULL + tag_size + ((id3[5] & 0x10) ? 10ULL : 0ULL);
        if (skip < size) {
            size -= skip;
            *data_offset = skip;
        }
    }
    fseek(file, saved, SEEK_SET);
    return size;
}

static u64 mp3_xing_duration_ms(const u8 *frame, int frame_size,
                               const mp3dec_frame_info_t *info) {
    if (!frame || !info || info->frame_offset < 0 ||
        info->frame_bytes <= 0 || info->frame_bytes > frame_size ||
        info->frame_offset > info->frame_bytes - 8)
        return 0;
    const u8 *header = frame + info->frame_offset;
    int frame_len = info->frame_bytes - info->frame_offset;
    if (frame_len < 8) return 0;
    bool mpeg1 = ((header[1] >> 3) & 3) == 3;
    bool mono = ((header[3] >> 6) & 3) == 3;
    int offset = 4 + ((header[1] & 1) ? 0 : 2) +
                 (mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17));
    if (offset + 8 > frame_len || offset + 8 > frame_size - info->frame_offset)
        return 0;
    const u8 *xing = header + offset;
    if (memcmp(xing, "Xing", 4) && memcmp(xing, "Info", 4)) return 0;
    u32 flags = ((u32)xing[4] << 24) | ((u32)xing[5] << 16) |
                ((u32)xing[6] << 8) | xing[7];
    int field = offset + 8;
    if (!(flags & 1) || field + 4 > frame_len) return 0;
    const u8 *entry = header + field;
    u32 frames = ((u32)entry[0] << 24) | ((u32)entry[1] << 16) |
                 ((u32)entry[2] << 8) | entry[3];
    field += 4;
    if (flags & 2) {
        if (field + 4 > frame_len) return 0;
        entry = header + field;
        mp3_xing_bytes = ((u32)entry[0] << 24) | ((u32)entry[1] << 16) |
                         ((u32)entry[2] << 8) | entry[3];
        field += 4;
    }
    if ((flags & 4) && mp3_xing_bytes && field + 100 <= frame_len) {
        memcpy(mp3_xing_toc, header + field, sizeof(mp3_xing_toc));
        mp3_toc_valid = true;
    }
    u32 samples = info->layer == 1 ? 384 : (mpeg1 ? 1152 : 576);
    return frames && info->hz > 0 ? (u64)frames * samples * 1000 / info->hz : 0;
}

static void update_playback_clock(u64 now) {
    if (!playback_last_update_ms) playback_last_update_ms = now;
    if (playing && !paused) {
        u64 old_second = playback_position_ms / 1000;
        // Follow the rate actually sent to NDSP, including the short pitch ramp.
        // Retain the fractional millisecond instead of losing it each UI frame.
        if (base_rate > 0 && applied_output_rate > 0) {
            u64 scaled = (now - playback_last_update_ms) * (u64)applied_output_rate +
                         playback_rate_remainder;
            playback_position_ms += scaled / (u64)base_rate;
            playback_rate_remainder = scaled % (u64)base_rate;
        }
        if (playback_position_ms / 1000 != old_second) time_dirty = true;
    }
    playback_last_update_ms = now;
}

static void print_track_time(u64 milliseconds) {
    u64 seconds = milliseconds / 1000;
    printf("%02llu:%02llu", (unsigned long long)(seconds / 60),
           (unsigned long long)(seconds % 60));
}

static void format_pitch_ratio(char *out, size_t capacity) {
    if (!out || capacity == 0) return;
    out[0] = '\0';
    if (capacity < 7) return;
    int value = pitch_milli;
    if (value < 250) value = 250;
    if (value > 2000) value = 2000;
    unsigned whole = (unsigned)value / 1000;
    unsigned fraction = (unsigned)value % 1000;
    out[0] = (char)('0' + whole);
    out[1] = '.';
    out[2] = (char)('0' + fraction / 100);
    out[3] = (char)('0' + (fraction / 10) % 10);
    if (fraction % 10 == 0) {
        out[4] = 'x';
        out[5] = '\0';
    } else {
        out[4] = (char)('0' + fraction % 10);
        out[5] = 'x';
        out[6] = '\0';
    }
}

static void draw_playback_time(void) {
    if (!playing) return;
    top_console.cursorX = 1;
    top_console.cursorY = playback_time_row;
    consoleSelect(&top_console);
    printf("Temps : ");
    print_track_time(playback_position_ms);
    printf("/");
    if (track_duration_ms) print_track_time(track_duration_ms);
    else printf("--:--");
    time_dirty = false;
}

static void draw_pitch_value(void) {
    if (!playing) return;
    char pitch[16];
    format_pitch_ratio(pitch, sizeof(pitch));
    consoleSelect(&top_console);
    top_console.cursorX = 1;
    top_console.cursorY = pitch_value_row;
    printf("Pitch + vitesse : %s   ", pitch);
    pitch_dirty = false;
}

static void draw_progress_bar(void) {
    if (!playing) return;
    u64 position = touch_seek_dragging ? touch_seek_target_ms : playback_position_ms;
    if (track_duration_ms && position > track_duration_ms) position = track_duration_ms;
    unsigned percent = track_duration_ms ?
        (unsigned)(position * 100 / track_duration_ms) : 0;
    char label[64];
    snprintf(label, sizeof(label), "%s %02llu:%02llu  %3u%%",
             touch_seek_dragging ? "Cible :" : "Position :",
             (unsigned long long)(position / 60000),
             (unsigned long long)((position / 1000) % 60), percent);
    consoleSelect(&bottom_console);
    bottom_console.cursorX = 1;
    bottom_console.cursorY = BOTTOM_PROGRESS_LABEL_ROW;
    printf("%-38.38s", label);
    bottom_console.cursorX = 1;
    bottom_console.cursorY = BOTTOM_PROGRESS_ROW;
    printf("[");
    unsigned filled = track_duration_ms ?
        (unsigned)(position * 36 / track_duration_ms) : 0;
    for (unsigned i = 0; i < 36; i++)
        putchar(touch_seek_dragging && i == filled ? '|' : i < filled ? '=' : '-');
    printf("]");
}

static u64 touch_target_ms(unsigned x) {
    if (x <= 8) return 0;
    if (x >= 312) return track_duration_ms;
    return (u64)(x - 8) * track_duration_ms / 304;
}

static unsigned reverb_touch_value(unsigned x) {
    if (reverb_custom_enabled) {
        if (x <= 40) return 0;
        if (x >= 280) return 100;
        return ((x - 40) * 100 + 120) / 240;
    }
    unsigned preset = x <= 40 ? 0 : x >= 280 ? 3 : (x - 40) / 60;
    return preset > 3 ? 3 : preset;
}

static void draw_reverb_bar(void) {
    if (!playing) return;
    consoleSelect(&bottom_console);
    unsigned value = touch_reverb_dragging ? touch_reverb_preview :
        reverb_custom_enabled ? reverb_custom_percent : reverb_mode;
    bottom_console.cursorX = 4;
    bottom_console.cursorY = BOTTOM_REVERB_LABEL_ROW;
    if (reverb_custom_enabled)
        printf("%-32.32s", shield_ui_language.french ?
               "Reverb sur mesure" : "Custom reverb");
    else
        printf("%-32.32s", shield_ui_translate(reverb_names[value]));
    bottom_console.cursorX = 4;
    bottom_console.cursorY = BOTTOM_REVERB_MARKER_ROW;
    if (reverb_custom_enabled)
        printf("0%%                         %3u%% ", value);
    else
        printf("   1       2       3       4   ");
    bottom_console.cursorX = 1;
    bottom_console.cursorY = BOTTOM_REVERB_ROW;
    printf("%-38s", "");
    bottom_console.cursorX = 4;
    printf("[");
    unsigned filled = reverb_custom_enabled ? value * 29 / 100 : (value * 8 + 3);
    for (unsigned i = 0; i < 30; ++i)
        putchar(i == filled ? '|' : i < filled ? '=' : '-');
    printf("]");
}

static void reset_reverb(void) {
    memset(reverb_comb, 0, sizeof(reverb_comb));
    memset(reverb_damping, 0, sizeof(reverb_damping));
    memset(reverb_allpass, 0, sizeof(reverb_allpass));
    reverb_smoothed[0] = reverb_smoothed[1] = 0;
    reverb_position = 0;
    reverb_wet_current_q15 = 0;
    reverb_dry_current_q15 = 32768;
}

static int32_t clamp_reverb_state(int64_t value) {
    if (value > 524287) return 524287;
    if (value < -524288) return -524288;
    return (int32_t)value;
}

static int32_t process_reverb_sample(int32_t input, int channel) {
    // Keep distinct tail lengths for each room preset.
    static const int32_t feedback_q15[] = { 0, 30890, 31550, 32010, 31690 };
    static const int32_t damping_q15[] = { 0, 24000, 22000, 20000, 22000 };
    int32_t sum = 0;
    unsigned mode = reverb_custom_enabled ? 4 : reverb_mode;
    // The optional longer decay changes feedback only; wet level stays fixed.
    static const int32_t long_add_q15[] = { 0, 800, 500, 400, 500 };
    int32_t feedback = feedback_q15[mode] +
        (reverb_long_enabled ? long_add_q15[mode] : 0);
    int32_t damping = damping_q15[mode];
    for (unsigned i = 0; i < 4; i++) {
        unsigned delay = reverb_comb_delay[channel][i];
        unsigned read = (reverb_position + REVERB_BUFFER_SIZE - delay) % REVERB_BUFFER_SIZE;
        int32_t delayed = reverb_comb[channel][i][read];
        int32_t damped = (int32_t)(((int64_t)delayed * (32768 - damping) +
                                    (int64_t)reverb_damping[channel][i] * damping) >> 15);
        reverb_damping[channel][i] = damped;
        reverb_comb[channel][i][reverb_position] =
            clamp_reverb_state(input + (((int64_t)damped * feedback) >> 15));
        sum += damped / 4;
    }
    int32_t wet = sum;
    for (unsigned i = 0; i < 3; i++) {
        unsigned delay = reverb_allpass_delay[channel][i];
        unsigned read = (reverb_position + REVERB_AP_BUFFER_SIZE - delay) % REVERB_AP_BUFFER_SIZE;
        int32_t delayed = reverb_allpass[channel][i][read];
        /* Schroeder all-pass: y[n] = x[n-D] - g*x[n],
         * xbuf[n] = x[n] + g*y[n]. Keep g at 0.5 for stable diffusion. */
        int32_t output = delayed - (wet >> 1);
        reverb_allpass[channel][i][reverb_position % REVERB_AP_BUFFER_SIZE] =
            clamp_reverb_state(wet + (output >> 1));
        wet = output;
    }
    return wet;
}

static int16_t limit_reverb_sample(int32_t sample) {
    int32_t magnitude = sample < 0 ? -sample : sample;
    if (magnitude > 30000) magnitude = 30000 + ((magnitude - 30000) >> 3);
    if (magnitude > 32760) magnitude = 32760;
    return (int16_t)(sample < 0 ? -magnitude : magnitude);
}

static void apply_reverb(int16_t *pcm, int frames) {
    /* Retain the dry signal and increase the delayed return moderately.
     * The output limiter guards peaks when effects are combined. */
    static const int32_t wet_q15[] = { 0, 17750, 19050, 21850 };
    int32_t wet = reverb_custom_enabled ?
        (int32_t)(reverb_custom_percent * 27000 / 100) : wet_q15[reverb_mode];
    if (!wet && !reverb_wet_current_q15 && reverb_dry_current_q15 == 32768) return;
    /* Keep most of the original signal: crossfading it away made the effect
     * sound quieter because the delayed return has lower RMS level. */
    int32_t dry_gain = 32768 - wet / 4;
    int32_t return_gain = wet * 3 / 4;
    for (int i = 0; i < frames; i++) {
        if (reverb_wet_current_q15 < wet) {
            reverb_wet_current_q15 += 32;
            if (reverb_wet_current_q15 > wet) reverb_wet_current_q15 = wet;
        } else if (reverb_wet_current_q15 > wet) {
            reverb_wet_current_q15 -= 32;
            if (reverb_wet_current_q15 < wet) reverb_wet_current_q15 = wet;
        }
        int32_t dry_target = 32768 - wet / (reverb_custom_enabled ? 2 : 4);
        if (reverb_dry_current_q15 < dry_target) {
            reverb_dry_current_q15 += 8;
            if (reverb_dry_current_q15 > dry_target) reverb_dry_current_q15 = dry_target;
        } else if (reverb_dry_current_q15 > dry_target) {
            reverb_dry_current_q15 -= 8;
            if (reverb_dry_current_q15 < dry_target) reverb_dry_current_q15 = dry_target;
        }
        dry_gain = reverb_dry_current_q15;
        return_gain = reverb_wet_current_q15 * 3 / 4;
        int32_t left = pcm[i * 2];
        int32_t right = pcm[i * 2 + 1];
        int32_t echo_left = process_reverb_sample(left, 0);
        int32_t echo_right = process_reverb_sample(right, 1);
        // A single inexpensive low-pass stage softens metallic upper harmonics.
        reverb_smoothed[0] += (echo_left - reverb_smoothed[0]) / 2;
        reverb_smoothed[1] += (echo_right - reverb_smoothed[1]) / 2;
        int32_t out_left = (int32_t)(((int64_t)left * dry_gain +
                            (int64_t)reverb_smoothed[0] * return_gain) >> 15);
        int32_t out_right = (int32_t)(((int64_t)right * dry_gain +
                             (int64_t)reverb_smoothed[1] * return_gain) >> 15);
        pcm[i * 2] = limit_reverb_sample(out_left);
        pcm[i * 2 + 1] = limit_reverb_sample(out_right);
        reverb_position = (reverb_position + 1) % REVERB_BUFFER_SIZE;
    }
}

/* Three broad bands made from two one-pole splits. All profile gains are
 * <= unity: the existing DSP bass boost remains independent and the EQ adds
 * no extra peak gain. The state and gain ramps persist between audio blocks. */
static void apply_filter(int16_t *pcm, int frames) {
    static const int32_t gains[7][3] = {
        {32768,32768,32768}, {32768,18500,32768},
        {23500,32768,30000}, {32768,31000,20500},
        {24000,31500,32768}, {21000,32768,32768},
        {30000,32768,23000}
    };
    if (!filter_profile && filter_gain_current[0] == 32768 &&
        filter_gain_current[1] == 32768 && filter_gain_current[2] == 32768) {
        filter_state_ready = false;
        return;
    }
    int rate = base_rate > 8000 ? base_rate : 44100;
    int32_t low_alpha = (int32_t)(32768.0f * 1382.3f / (rate + 1382.3f));
    int32_t high_alpha = (int32_t)(32768.0f * 18849.6f / (rate + 18849.6f));
    if (low_alpha < 1) low_alpha = 1;
    if (high_alpha < 1) high_alpha = 1;
    for (int i = 0; i < frames; i++) {
        for (unsigned band = 0; band < 3; band++) {
            int32_t target = gains[filter_profile][band];
            int32_t *current = &filter_gain_current[band];
            if (*current < target) {
                *current += 48;
                if (*current > target) *current = target;
            } else if (*current > target) {
                *current -= 48;
                if (*current < target) *current = target;
            }
        }
        for (int ch = 0; ch < 2; ch++) {
            int32_t sample = pcm[i*2+ch];
            if (!filter_state_ready) {
                filter_low_state[ch] = sample;
                filter_high_state[ch] = sample;
            }
            filter_low_state[ch] += (int32_t)(((int64_t)(sample-filter_low_state[ch])*low_alpha)>>15);
            filter_high_state[ch] += (int32_t)(((int64_t)(sample-filter_high_state[ch])*high_alpha)>>15);
            int32_t low = filter_low_state[ch];
            int32_t mid = filter_high_state[ch] - low;
            int32_t high = sample - filter_high_state[ch];
            int32_t out = (int32_t)(((int64_t)low*filter_gain_current[0] +
                (int64_t)mid*filter_gain_current[1] +
                (int64_t)high*filter_gain_current[2]) >> 15);
            if (out > 32767) out = 32767;
            if (out < -32768) out = -32768;
            pcm[i*2+ch] = (int16_t)out;
        }
        filter_state_ready = true;
    }
    if (!filter_profile && filter_gain_current[0] == 32768 &&
        filter_gain_current[1] == 32768 && filter_gain_current[2] == 32768)
        filter_state_ready = false;
}

static void apply_audio_boost(int16_t *pcm, int frames) {
    if (frames <= 0) return;
    if (!audio_boost_mode) {
        boost_gain_q15 = 32768;
        return;
    }
    static const int32_t gain_levels_q15[] = { 32768, 65536, 98304, 163840 };
    // A single loud sample used to hold an entire 8192-frame block down.
    // Small look-ahead windows boost the quiet passages without clipping a
    // later transient; gain recovery is continuous within each window.
    for (int pos = 0; pos < frames; ) {
        int count = frames - pos;
        if (count > 256) count = 256;
        int32_t peak = 0;
        for (int i = 0; i < count * 2; ++i) {
            int32_t sample = pcm[pos * 2 + i];
            int32_t magnitude = sample < 0 ? -sample : sample;
            if (magnitude > peak) peak = magnitude;
        }
        int32_t target = gain_levels_q15[audio_boost_mode];
        if (peak > 0 && (int64_t)peak * target > (int64_t)32760 * 32768)
            target = (int32_t)((int64_t)32760 * 32768 / peak);
        int32_t next_target = target;
        if (pos + count < frames) {
            int next_count = frames - pos - count;
            if (next_count > 256) next_count = 256;
            int32_t next_peak = 0;
            for (int i = 0; i < next_count * 2; ++i) {
                int32_t sample = pcm[(pos + count) * 2 + i];
                int32_t magnitude = sample < 0 ? -sample : sample;
                if (magnitude > next_peak) next_peak = magnitude;
            }
            if (next_peak > 0 && (int64_t)next_peak * next_target > (int64_t)32760 * 32768)
                next_target = (int32_t)((int64_t)32760 * 32768 / next_peak);
        }
        int32_t start = boost_gain_q15;
        if (target < start) start = target;
        int32_t end = start;
        if (target > start) {
            int32_t step = (target - start) / 32;
            end += step > 0 ? step : 1;
        }
        if (next_target < end) end = next_target;
        int64_t gain = (int64_t)start << 16;
        int64_t gain_step = ((int64_t)(end - start) << 16) / count;
        for (int i = 0; i < count; ++i) {
            int32_t current = (int32_t)(gain >> 16);
            pcm[(pos + i) * 2] = (int16_t)(((int64_t)pcm[(pos + i) * 2] * current) >> 15);
            pcm[(pos + i) * 2 + 1] = (int16_t)(((int64_t)pcm[(pos + i) * 2 + 1] * current) >> 15);
            gain += gain_step;
        }
        boost_gain_q15 = end;
        pos += count;
    }
}

static void fade_out_block(AudioBlock *block, int frames) {
    if (!block || !block->pcm || frames <= 0) return;
    int fade_frames = (int)(base_rate * speed * TRACK_FADE_MS / 1000.0f);
    if (fade_frames < 1) fade_frames = 1;
    if (fade_frames > frames) fade_frames = frames;
    int start = frames - fade_frames;
    for (int i = start; i < frames; i++) {
        int gain = frames - i - 1;
        block->pcm[i * 2] = (int16_t)((int32_t)block->pcm[i * 2] * gain / fade_frames);
        block->pcm[i * 2 + 1] = (int16_t)((int32_t)block->pcm[i * 2 + 1] * gain / fade_frames);
    }
    DSP_FlushDataCache(block->pcm + start * 2, fade_frames * 2 * sizeof(int16_t));
}

static void fade_in_block(AudioBlock *block, int frames) {
    if (fade_in_remaining < 0) {
        fade_in_total = (int)(base_rate * speed * TRACK_FADE_MS / 1000.0f);
        if (fade_in_total < 1) fade_in_total = 1;
        fade_in_remaining = fade_in_total;
    }
    for (int i = 0; i < frames && fade_in_remaining > 0; i++) {
        int gain = fade_in_total - fade_in_remaining;
        block->pcm[i * 2] = (int16_t)((int32_t)block->pcm[i * 2] * gain / fade_in_total);
        block->pcm[i * 2 + 1] = (int16_t)((int32_t)block->pcm[i * 2 + 1] * gain / fade_in_total);
        fade_in_remaining--;
    }
}

static void update_battery(void) {
    u8 whole, fraction;
    if (battery_service_ready &&
        R_SUCCEEDED(MCUHWC_ReadRegister(0x0B, &whole, 1)) && whole <= 100 &&
        R_SUCCEEDED(MCUHWC_ReadRegister(0x0C, &fraction, 1))) {
        battery_percent = whole + fraction / 256.0f;
    } else {
        battery_percent = -1.0f;
    }
}

static void update_charging_state(void) {
    u8 state;
    if (ptmu_service_ready && R_SUCCEEDED(PTMU_GetBatteryChargeState(&state))) {
        battery_charging = state != 0;
        charging_state_known = true;
    } else {
        charging_state_known = false;
    }
}

static void update_battery_temperature(void) {
    u8 temperature;
    if (battery_service_ready &&
        R_SUCCEEDED(MCUHWC_ReadRegister(0x0A, &temperature, 1)) &&
        temperature <= 100) {
        battery_temperature_c = temperature;
    } else {
        battery_temperature_c = -1;
    }
}

static void update_sd_storage(void) {
    FS_ArchiveResource resource = {0};
    sd_storage_ready = fs_service_ready &&
        R_SUCCEEDED(FSUSER_GetSdmcArchiveResource(&resource)) &&
        resource.clusterSize > 0;
    if (sd_storage_ready) {
        sd_total_bytes = (u64)resource.clusterSize * resource.totalClusters;
        sd_free_bytes = (u64)resource.clusterSize * resource.freeClusters;
    }
}

// Boost the audible kick/bass band without lifting the entire low-mid range.
// Y cycles through five profiles; the filter itself runs on the DSP.
static void apply_tone(void) {
    static const float gains_db[] = { 0.0f, 6.0f, 9.0f, 16.0f, 22.0f };
    if (bass_profile % 5 == 0) {
        ndspChnIirBiquadSetEnable(CHANNEL, false);
        return;
    }
    const float gain_db = gains_db[bass_profile % 5];
    const float A = powf(10.0f, gain_db / 40.0f);
    const float w0 = 6.28318530718f * 180.0f / NDSP_SAMPLE_RATE;
    const float c = cosf(w0), alpha = sinf(w0) / (2.0f * 0.8f);
    const float a0 = 1.0f + alpha / A;
    const float a1 = -2.0f * c;
    const float a2 = 1.0f - alpha / A;
    const float b0 = 1.0f + alpha * A;
    const float b1 = -2.0f * c;
    const float b2 = 1.0f - alpha * A;
    if (ndspChnIirBiquadSetParamsCustomFilter(CHANNEL, a0, a1, a2, b0, b1, b2))
        ndspChnIirBiquadSetEnable(CHANNEL, true);
}

static bool has_extension(const char *name, const char *extension) {
    const char *dot = strrchr(name, '.');
    if (!dot || strlen(dot) != strlen(extension)) return false;
    while (*dot && *extension) {
        char a = *dot++, b = *extension++;
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return !*dot && !*extension;
}

static bool is_audio_file(const char *name) {
    return has_extension(name, ".mp3") || has_extension(name, ".ogg");
}

static int compare_browse_entries(const void *left, const void *right) {
    const BrowseEntry *a = (const BrowseEntry *)left;
    const BrowseEntry *b = (const BrowseEntry *)right;
    if (a->directory != b->directory) return a->directory ? -1 : 1;
    return strcasecmp(a->name, b->name);
}

static void stop_track(bool allow_sleep) {
    if (audio_ready) {
        ndspChnWaveBufClear(CHANNEL);
        ndspChnSetPaused(CHANNEL, false);
    }
    applied_output_rate = 0;
    target_output_rate = 0;
    boost_gain_q15 = 32768;
    last_queued_block = NULL;
    if (ogg_open) {
        ov_clear(&ogg_decoder); // also closes the FILE passed to ov_open
        ogg_open = false;
    } else if (audio_file) {
        fclose(audio_file);
    }
    audio_file = NULL;
    ogg_track = false;
    reset_reverb();
    filter_state_ready = false;
    playing = false;
    paused = false;
    touch_seek_dragging = false;
    touch_reverb_dragging = false;
    time_dirty = false;
    pitch_dirty = false;
    playback_status = "";
    aptSetSleepAllowed(allow_sleep);
    ui_dirty = true;
}

static void scan_folder(void) {
    track_count = 0;
    entry_count = 0;
    DIR *dir = opendir(folder);
    if (!dir) {
        playback_status = "Impossible d'ouvrir ce dossier SD.";
        return;
    }
    if (!playing) playback_status = "";
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && track_count < MAX_TRACKS) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        bool is_audio = is_audio_file(entry->d_name);
        bool is_directory = false;
        if (!is_audio) {
            char child[PATH_CAP + 256];
            snprintf(child, sizeof(child), "%s%s%s", folder,
                     folder[strlen(folder)-1] == '/' ? "" : "/", entry->d_name);
            struct stat st;
            is_directory = stat(child, &st) == 0 && S_ISDIR(st.st_mode);
        }
        if ((is_directory || is_audio) && entry_count < MAX_TRACKS) {
            copy_name(entries[entry_count].name, sizeof(entries[entry_count].name), entry->d_name);
            entries[entry_count].directory = is_directory;
            entry_count++;
        }
        if (is_audio) {
            copy_name(tracks[track_count].name, sizeof(tracks[track_count].name), entry->d_name);
            track_count++;
        }
    }
    closedir(dir);
    qsort(entries, entry_count, sizeof(entries[0]), compare_browse_entries);
}

static void enter_selected_directory(void) {
    if (selection < 0 || selection >= entry_count || !entries[selection].directory) return;
    size_t n = strlen(folder);
    if (n + strlen(entries[selection].name) + 2 >= sizeof(folder)) return;
    if (folder[n-1] != '/') strcat(folder, "/");
    strcat(folder, entries[selection].name);
    strcat(folder, "/");
    selection = 0;
    scan_folder();
}

static void parent_directory(void) {
    size_t n = strlen(folder);
    while (n > 0 && folder[n-1] == '/') folder[--n] = 0;
    while (n > 0 && folder[n-1] != '/') folder[--n] = 0;
    if (n == 0) strcpy(folder, "sdmc:/");
    selection = 0;
    scan_folder();
}

static void build_order(int first) {
    for (int i = 0; i < track_count; i++) order[i] = i;
    if (shuffle) {
        for (int i = track_count - 1; i > 0; i--) {
            int j = rand() % (i + 1), t = order[i]; order[i] = order[j]; order[j] = t;
        }
        if (first >= 0) {
            for (int i = 0; i < track_count; i++) if (order[i] == first) {
                int t = order[0]; order[0] = order[i]; order[i] = t; break;
            }
        }
    }
}

static bool queue_block(AudioBlock *b) {
    if (!b->pcm) return false;
    memset(&b->wave, 0, sizeof(b->wave));
    int frames = 0;
    if (ogg_track) {
        int read_errors = 0;
        while (frames < BLOCK_FRAMES) {
            int bitstream = 0;
            int remaining = BLOCK_FRAMES - frames;
            int frame_bytes = ogg_channels * (int)sizeof(int16_t);
            int request_bytes = remaining * frame_bytes;
            if (request_bytes > (int)sizeof(ogg_pcm))
                request_bytes = sizeof(ogg_pcm);
            long bytes = ov_read(&ogg_decoder, (char *)ogg_pcm, request_bytes, &bitstream);
            if (bytes < 0 && ++read_errors < 4) continue;
            if (bytes <= 0) {
                audio_eof = true;
                break;
            }
            read_errors = 0;
            if (bytes > request_bytes || bytes % frame_bytes) {
                audio_eof = true;
                break;
            }
            int count = (int)bytes / frame_bytes;
            if (count <= 0 || count > remaining) {
                audio_eof = true;
                break;
            }
            for (int i = 0; i < count; i++) {
                int16_t left = ogg_pcm[i * ogg_channels];
                int16_t right = ogg_channels == 1 ? left : ogg_pcm[i * 2 + 1];
                b->pcm[(frames + i) * 2] = left;
                b->pcm[(frames + i) * 2 + 1] = right;
            }
            frames += count;
            track_has_audio = true;
        }
    } else {
    int skipped_bytes = 0;
    while (frames < BLOCK_FRAMES) {
        if (pending_pos < pending_frames) {
            int take = pending_frames - pending_pos;
            if (take > BLOCK_FRAMES - frames) take = BLOCK_FRAMES - frames;
            memcpy(b->pcm + frames * 2, pending_pcm + pending_pos * 2,
                   take * 2 * sizeof(int16_t));
            pending_pos += take;
            frames += take;
            continue;
        }
        pending_frames = pending_pos = 0;
        if (input_size < 4096 && !feof(audio_file)) {
            int got = (int)fread(input + input_size, 1, INPUT_CAP - input_size, audio_file);
            input_size += got;
        }
        if (input_size < 4) {
            if (feof(audio_file)) audio_eof = true;
            break;
        }
        mp3dec_frame_info_t info;
        int count = mp3dec_decode_frame(&decoder, input, input_size, decoded, &info);
        if (!track_duration_ms && info.frame_bytes > 0 &&
            info.frame_bytes <= input_size)
            track_duration_ms = mp3_xing_duration_ms(input, input_size, &info);
        if (!track_duration_ms && count > 0 && audio_payload_bytes &&
            info.bitrate_kbps > 0)
            track_duration_ms = audio_payload_bytes * 8 / (u32)info.bitrate_kbps;
        if (info.frame_bytes <= 0 || info.frame_bytes > input_size) {
            memmove(input, input + 1, --input_size);
            if (++skipped_bytes > MP3_BAD_DATA_LIMIT) { audio_eof = true; break; }
            continue;
        }
        memmove(input, input + info.frame_bytes, input_size - info.frame_bytes);
        input_size -= info.frame_bytes;
        if (count <= 0 || (info.channels != 1 && info.channels != 2)) {
            skipped_bytes += info.frame_bytes;
            if (skipped_bytes > MP3_BAD_DATA_LIMIT) { audio_eof = true; break; }
            continue;
        }
        skipped_bytes = 0;
        track_has_audio = true;
        for (int i = 0; i < count; i++) {
            int left = info.channels == 1 ? decoded[i] : decoded[i*2];
            int right = info.channels == 1 ? left : decoded[i*2+1];
            pending_pcm[i*2] = (int16_t)left;
            pending_pcm[i*2+1] = (int16_t)right;
        }
        pending_frames = count;
        base_rate = info.hz;
        update_output_rate();
    }
    }
    if (!frames) {
        audio_eof = true;
        if (last_queued_block && last_queued_block->wave.status == NDSP_WBUF_QUEUED)
            fade_out_block(last_queued_block, last_queued_block->wave.nsamples);
        return false;
    }
    apply_filter(b->pcm, frames);
    apply_reverb(b->pcm, frames);
    apply_audio_boost(b->pcm, frames);
    fade_in_block(b, frames);
    if (audio_eof) fade_out_block(b, frames);
    DSP_FlushDataCache(b->pcm, frames * 2 * sizeof(int16_t));
    b->wave.data_vaddr = b->pcm;
    // NDSP's nsamples is the number of sample frames per channel for PCM.
    // Stereo data is interleaved in memory, but must not double this value.
    b->wave.nsamples = frames;
    b->wave.looping = false;
    ndspChnWaveBufAdd(CHANNEL, &b->wave);
    last_queued_block = b;
    return true;
}

static bool open_track(int pos) {
    // Keep sleep blocked across a track handoff: a closed 3DS may enter full
    // sleep during the brief gap before the next track is queued.
    stop_track(false);
    if (!audio_ready || pos < 0 || pos >= track_count) return false;
    order_pos = pos;
    playback_position_ms = 0;
    playback_rate_remainder = 0;
    playback_last_update_ms = 0;
    track_duration_ms = 0;
    audio_payload_bytes = 0;
    mp3_data_offset = 0;
    mp3_xing_bytes = 0;
    mp3_toc_valid = false;
    char path[PATH_CAP + 256];
    snprintf(path, sizeof(path), "%s%s%s", folder,
             folder[strlen(folder)-1] == '/' ? "" : "/", tracks[order[pos]].name);
    audio_file = fopen(path, "rb");
    if (!audio_file) {
        playback_status = "Impossible d'ouvrir ce fichier audio.";
        aptSetSleepAllowed(true);
        return false;
    }
    copy_name(current_title, sizeof(current_title), tracks[order[pos]].name);
    ogg_track = has_extension(tracks[order[pos]].name, ".ogg");
    if (!ogg_track) audio_payload_bytes = get_payload_bytes(audio_file, &mp3_data_offset);
    if (ogg_track) {
        if (ov_open(audio_file, &ogg_decoder, NULL, 0) != 0) {
            fclose(audio_file);
            audio_file = NULL;
            playback_status = "Fichier OGG/Vorbis invalide ou non pris en charge.";
            aptSetSleepAllowed(true);
            return false;
        }
        ogg_open = true;
        vorbis_info *info = ov_info(&ogg_decoder, -1);
        if (!info || (info->channels != 1 && info->channels != 2) ||
            info->rate < 8000 || info->rate > 48000) {
            stop_track(true);
            playback_status = "OGG invalide : mono/stereo et 8-48 kHz requis.";
            return false;
        }
        ogg_channels = info->channels;
        base_rate = (int)info->rate;
        ogg_int64_t duration = ov_time_total(&ogg_decoder, -1);
        if (duration > 0) track_duration_ms = (u64)duration;
        update_output_rate();
    } else {
        if (mp3_data_offset > LONG_MAX ||
            fseek(audio_file, (long)mp3_data_offset, SEEK_SET) != 0) {
            stop_track(true);
            playback_status = "En-tete MP3 invalide ou trop grand.";
            return false;
        }
        mp3dec_init(&decoder);
    }
    input_size = 0;
    pending_frames = pending_pos = 0;
    audio_eof = false;
    fade_in_remaining = -1;
    fade_in_total = 0;
    track_has_audio = false;
    ndspChnSetPaused(CHANNEL, true);
    for (int i = 0; i < AUDIO_BLOCK_COUNT && !audio_eof; i++)
        queue_block(&blocks[i]);
    playing = true;
    playback_last_update_ms = osGetTime();
    ndspChnSetPaused(CHANNEL, false);
    playback_status = "";
    aptSetSleepAllowed(false);
    ui_dirty = true;
    return true;
}

static bool seek_track(u64 target_ms) {
    if (!playing || !audio_file || !track_duration_ms) return false;
    if (target_ms >= track_duration_ms)
        target_ms = track_duration_ms > 1000 ? track_duration_ms - 1000 : 0;
    ndspChnSetPaused(CHANNEL, true);
    if (ogg_track) {
        if (ov_time_seek(&ogg_decoder, (ogg_int64_t)target_ms) != 0) {
            stop_track(true);
            playback_status = "Cet OGG ne permet pas ce deplacement.";
            ui_dirty = true;
            return false;
        }
    } else {
        u64 data_bytes = audio_payload_bytes;
        if (!data_bytes) {
            ndspChnSetPaused(CHANNEL, paused);
            playback_last_update_ms = osGetTime();
            return false;
        }
        u64 position = data_bytes * target_ms / track_duration_ms;
        if (mp3_toc_valid && mp3_xing_bytes) {
            u64 thousandths = target_ms * 100000 / track_duration_ms;
            unsigned index = (unsigned)(thousandths / 1000);
            if (index > 99) index = 99;
            unsigned fraction = (unsigned)(thousandths % 1000);
            unsigned first = mp3_xing_toc[index];
            unsigned second = index == 99 ? 256 : mp3_xing_toc[index + 1];
            if (second < first) second = first;
            position = mp3_xing_bytes * (first * 1000ULL +
                       (second - first) * (u64)fraction) / 256000ULL;
        }
        if (data_bytes > 4096 && position > data_bytes - 4096)
            position = data_bytes - 4096;
        if (mp3_data_offset + position > LONG_MAX ||
            fseek(audio_file, (long)(mp3_data_offset + position), SEEK_SET) != 0) {
            playback_status = "Impossible de se deplacer dans ce MP3.";
            ndspChnSetPaused(CHANNEL, paused);
            playback_last_update_ms = osGetTime();
            ui_dirty = true;
            return false;
        }
        mp3dec_init(&decoder);
        input_size = pending_frames = pending_pos = 0;
    }
    ndspChnWaveBufClear(CHANNEL);
    last_queued_block = NULL;
    reset_reverb();
    filter_state_ready = false;
    boost_gain_q15 = 32768;
    audio_eof = false;
    fade_in_remaining = -1;
    track_has_audio = false;
    playback_position_ms = target_ms;
    playback_rate_remainder = 0;
    for (int i = 0; i < AUDIO_BLOCK_COUNT && !audio_eof; i++)
        queue_block(&blocks[i]);
    playback_last_update_ms = osGetTime();
    ndspChnSetPaused(CHANNEL, paused);
    playback_status = ogg_track ? "Position modifiee." :
                    "Position MP3 approximative.";
    ui_dirty = true;
    return true;
}

static void start_folder(int first) {
    stop_track(false);
    failed_tracks_in_a_row = 0;
    if (!track_count) {
        aptSetSleepAllowed(true);
        playback_status = "Aucun MP3 ou OGG dans ce dossier.";
        ui_dirty = true;
        return;
    }
    build_order(first);
    open_track(0);
}

static void step_track(int direction) {
    if (!track_count) return;
    int next = order_pos + direction;
    if (next >= track_count || next < 0) {
        int previous_track = order_pos >= 0 && order_pos < track_count ?
                             order[order_pos] : -1;
        if (shuffle) {
            build_order(-1);
            if (track_count > 1 && direction > 0 && order[0] == previous_track) {
                int t = order[0]; order[0] = order[1]; order[1] = t;
            } else if (track_count > 1 && direction < 0 &&
                       order[track_count - 1] == previous_track) {
                int t = order[track_count - 1];
                order[track_count - 1] = order[track_count - 2];
                order[track_count - 2] = t;
            }
        }
        next = direction > 0 ? 0 : track_count - 1;
    }
    open_track(next);
}

static void update_clock(u64 now_ms) {
    if (now_ms < clock_next_update_ms) return;

    time_t wall_time = time(NULL);
    char next_text[sizeof(clock_text)] = "--:--";
    u64 delay_ms = 60000;
    if (wall_time >= 0) {
        struct tm *local_time = localtime(&wall_time);
        if (local_time)
            snprintf(next_text, sizeof(next_text), "%02d:%02d",
                     local_time->tm_hour, local_time->tm_min);
        delay_ms = (u64)(60 - (wall_time % 60)) * 1000;
    }
    if (strcmp(clock_text, next_text) != 0) {
        memcpy(clock_text, next_text, sizeof(clock_text));
        ui_dirty = true;
    }
    clock_next_update_ms = now_ms + delay_ms;
}

static void draw_options(void) {
    consoleSelect(&bottom_console);
    bottom_console.cursorX = 1;
    bottom_console.cursorY = 1;
    printf(CONSOLE_CYAN "OPTIONS" CONSOLE_RESET "\n");
    printf("========================================\n");
    if (options_page == 0) {
        bottom_console.cursorX = 2; bottom_console.cursorY = 7;
        printf("%s %s", options_selection == 0 ? ">" : " ",
               shield_ui_language.french ? "Filtres" : "Filters");
        bottom_console.cursorX = 2; bottom_console.cursorY = 11;
        printf("%s %s", options_selection == 1 ? ">" : " ",
               shield_ui_language.french ? "Effets" : "Effects");
        bottom_console.cursorX = 1; bottom_console.cursorY = 25;
        printf(shield_ui_language.french ? "A ouvrir  B/SELECT fermer" :
                                           "A open  B/SELECT close");
    } else if (options_page == 1) {
        bottom_console.cursorX = 2; bottom_console.cursorY = 4;
        printf(shield_ui_language.french ? "Filtres : un seul a la fois" :
                                           "Filters: one at a time");
        for (unsigned i = 0; i < 7; i++) {
            bottom_console.cursorX = 2;
            bottom_console.cursorY = 6 + i * 3;
            printf("%s %-12s %s", options_selection == i ? ">" : " ",
                   shield_ui_translate(filter_names[i]),
                   filter_profile == i ? "[x]" : "[ ]");
        }
        bottom_console.cursorX = 1; bottom_console.cursorY = 27;
        printf(shield_ui_language.french ? "A choisir  B retour  SELECT fermer" :
                                           "A choose  B back  SELECT close");
    } else {
        bottom_console.cursorX = 2; bottom_console.cursorY = 5;
        printf(shield_ui_language.french ? "Effets" : "Effects");
        bottom_console.cursorX = 2; bottom_console.cursorY = 9;
        printf("%s %s : %s", options_selection == 0 ? ">" : " ", shield_ui_language.french ?
               "Reverb sur mesure" : "Custom reverb",
               reverb_custom_enabled ? "ON" : "OFF");
        bottom_console.cursorX = 2; bottom_console.cursorY = 13;
        printf("%s %s : %s", options_selection == 1 ? ">" : " ",
               shield_ui_language.french ? "Reverb prolongee" : "Long reverb",
               reverb_long_enabled ? "ON" : "OFF");
        bottom_console.cursorX = 2; bottom_console.cursorY = 17;
        printf(shield_ui_language.french ? "Libre ON : intensite tactile" :
                                           "Custom ON: touch intensity");
        bottom_console.cursorX = 2; bottom_console.cursorY = 20;
        printf(shield_ui_language.french ? "Libre OFF : 4 profils tactiles" :
                                           "Custom OFF: 4 touch presets");
        bottom_console.cursorX = 2; bottom_console.cursorY = 23;
        printf("1 Off  2 Concert  3 Cath.  4 Extra");
        bottom_console.cursorX = 1; bottom_console.cursorY = 27;
        printf(shield_ui_language.french ? "A basculer  B retour  SELECT fermer" :
                                           "A toggle  B back  SELECT close");
    }
}

static void options_activate(void) {
    if (options_page == 0) {
        options_page = options_selection == 0 ? 1 : 2;
        options_selection = 0;
    } else if (options_page == 1) {
        filter_profile = options_selection;
    } else {
        if (options_selection == 0) reverb_custom_enabled = !reverb_custom_enabled;
        else reverb_long_enabled = !reverb_long_enabled;
    }
    ui_dirty = true;
}

static void draw(void) {
    top_console.cursorX = 1;
    top_console.cursorY = 1;
    consoleSelect(&top_console);
    consoleClear();
    printf(CONSOLE_CYAN "SHIELD DJ LITE BY SHIELD-77 1.7.4" CONSOLE_RESET);
    top_console.cursorX = top_console.windowWidth > 5
        ? top_console.windowWidth - 5 : 0;
    top_console.cursorY = 0;
    printf("%s\n", clock_text);
    top_console.cursorX = 1;
    top_console.cursorY = 2;
    if (battery_percent >= 0.0f)
        printf("Batterie : %d%%\n", ((int)battery_percent / 5) * 5);
    else
        printf("Batterie : indisponible\n");
    if (battery_percent >= 0.0f && battery_percent <= 20.0f &&
        charging_state_known && !battery_charging)
        printf(CONSOLE_RED "BATTERIE FAIBLE - recharge la console" CONSOLE_RESET "\n");
    printf("Lecteur audio MP3 / OGG Vorbis\n");
    printf("========================================\n\n");
    printf(CONSOLE_YELLOW "DOSSIER" CONSOLE_RESET "\n");
    print_short(folder, 47);
    printf("\n\n");
    if (!audio_ready) {
        printf(CONSOLE_RED "SON INDISPONIBLE" CONSOLE_RESET "\n");
        printf("Cause : %s\n", shield_ui_translate(audio_status));
        printf("Lance DSP1 depuis Homebrew Launcher, puis\n");
        printf("relance Shield DJ Lite by Shield-77.\n\n");
    } else {
        printf(CONSOLE_GREEN "AUDIO PRET" CONSOLE_RESET "\n\n");
    }
    printf("Etat : %s\n", shield_ui_translate(
               playing ? (paused ? "En pause" : "Lecture") : "A l'arret"));
    printf("Piste : ");
    print_short(shield_ui_translate(current_title), 40);
    printf("\n");
    playback_time_row = playing ? top_console.cursorY : 0;
    if (playing) {
        draw_playback_time();
        printf("\n");
    }
    if (playback_status[0])
        printf(CONSOLE_YELLOW "%s\n" CONSOLE_RESET,
               shield_ui_translate(playback_status));
    top_console.cursorX = 1;
    top_console.cursorY = top_console.windowHeight - 11;
    printf(CONSOLE_YELLOW "REGLAGES" CONSOLE_RESET "\n");
    pitch_value_row = top_console.cursorY;
    char pitch[16];
    format_pitch_ratio(pitch, sizeof(pitch));
    printf("Pitch + vitesse : %s\n", pitch);
    printf("Profil basses : %s\n", shield_ui_translate(
           bass_profile == 4 ? "EXTREME +22 dB" :
           bass_profile == 3 ? "+16 dB" :
           bass_profile == 2 ? "BOOST +9 dB" :
           bass_profile == 1 ? "DOUX +6 dB" : "OFF"));
    if (reverb_custom_enabled)
        printf(shield_ui_language.french ?
               "Reverb : LIBRE %u%% | Filtre : %s\n" :
               "Reverb: CUSTOM %u%% | Filter: %s\n", reverb_custom_percent,
               shield_ui_translate(filter_names[filter_profile]));
    else
        printf(shield_ui_language.french ?
               "Reverb : %s | Filtre : %s\n" :
               "Reverb: %s | Filter: %s\n",
               shield_ui_translate(reverb_names[reverb_mode]),
               shield_ui_translate(filter_names[filter_profile]));
    printf("Luminosite : %d/5\n", brightness_level);
    printf("Shuffle : %s\n", shield_ui_translate(shuffle ? "active" : "desactive"));
    printf("Touches L/R : %s\n", shield_ui_translate(track_keys_locked ? "BLOQUEES" : "ACTIVES"));
    printf("Repeter piste : %s (X)\n", shield_ui_translate(repeat_current ? "OUI" : "NON"));
    printf("Boost son : %s (HOME)\n", audio_boost_mode == 3 ? "500%" :
           audio_boost_mode == 2 ? "300%" :
           audio_boost_mode == 1 ? "200%" : "OFF");
    top_console.cursorX = 1;
    if (sd_storage_ready) {
        const u64 gb = 1024ULL * 1024ULL * 1024ULL;
        u64 total_hundredths = sd_total_bytes * 100 / gb;
        u64 free_hundredths = sd_free_bytes * 100 / gb;
        printf("SD : %llu.%02llu Go libres / %llu.%02llu Go",
               (unsigned long long)(free_hundredths / 100),
               (unsigned long long)(free_hundredths % 100),
               (unsigned long long)(total_hundredths / 100),
               (unsigned long long)(total_hundredths % 100));
    } else {
        printf("SD : espace indisponible");
    }
    if (battery_temperature_c >= thermal_warning_threshold) {
        top_console.cursorX = 1;
        top_console.cursorY = top_console.windowHeight - 1;
        printf(CONSOLE_RED "SURCHAUFFE THERMIQUE EN COURS" CONSOLE_RESET);
    }

    bottom_console.cursorX = 1;
    bottom_console.cursorY = 1;
    consoleSelect(&bottom_console);
    consoleClear();
    if (options_open) {
        draw_options();
        ui_dirty = false;
        pitch_dirty = false;
        return;
    }
    printf(CONSOLE_CYAN "BIBLIOTHEQUE" CONSOLE_RESET "\n");
    printf("========================================\n");
    printf("Fichiers audio dans ce dossier : %d\n\n", track_count);
    if (!playing) {
        printf("Contenu :\n");
        int start = selection > 5 ? selection - 5 : 0;
        for (int i = start; i < entry_count && i < start + 6; i++) {
            if (i == selection) printf(CONSOLE_YELLOW "> " CONSOLE_RESET);
            else printf("  ");
            print_short(entries[i].name, entries[i].directory ? 34 : 36);
            printf("%s\n", entries[i].directory ? "/" : "");
            printf("\n\n");
        }
    } else {
        printf(shield_ui_language.french ?
               "A Pause   B Stop   L/R Piste\n\n"
               "X Repeter   Y Basses\n\n"
               "G/D Pitch    H/B Luminosite\n\n"
               "SELECT Options  HOME Boost\n\n" :
               "A Pause   B Stop   L/R Track\n\n"
               "X Repeat  Y Bass\n\n"
               "L/R Pitch  U/D Brightness\n\n"
               "SELECT Options  HOME Boost\n\n");
        printf(shield_ui_language.french ? "Trames perdues : %lu\n" :
                                           "Dropped frames: %lu\n",
               (unsigned long)ndspGetDroppedFrames());
    }
    if (!playing) {
        printf("\nHaut/Bas choisir | A ouvrir/lire\n");
        printf("B parent | Y lire dossier\n");
        printf(shield_ui_language.french ? "SELECT options\n" : "SELECT options\n");
        printf("START quitter\n");
    } else {
        bottom_console.cursorX = 1;
        bottom_console.cursorY = BOTTOM_START_ROW;
        printf("START quitter");
        bottom_console.cursorX = 1;
        bottom_console.cursorY = BOTTOM_START_ROW + 1;
        printf("Glisser sur la barre, puis relacher");
        draw_reverb_bar();
        draw_progress_bar();
    }
    bottom_console.cursorX = 1;
    bottom_console.cursorY = BOTTOM_TEMP_ROW;
    if (battery_temperature_c >= 0)
        printf("Temp. batt. : %d deg C", battery_temperature_c);
    else
        printf("Temp. batt. : indisponible");
    ui_dirty = false;
    pitch_dirty = false;
}

static void change_brightness(int delta) {
    if (!lcd_ready) return;
    int next = brightness_level + delta;
    if (next < 1) next = 1;
    if (next > 5) next = 5;
    if (next == brightness_level) return;
    if (R_SUCCEEDED(GSPLCD_SetBrightness(GSPLCD_SCREEN_BOTH, (u32)next))) {
        brightness_level = next;
        brightness_changed = true;
        ui_dirty = true;
    }
}

static void update_track_key_lock(u64 now) {
    static int previous_sector = -1, progress;
    static bool wait_for_center, gesture_active;
    static u64 last_step_ms, gesture_start_ms;
    circlePosition circle;
    hidCircleRead(&circle);
    int dx = circle.dx, dy = circle.dy;
    int ax = abs(dx), ay = abs(dy);
    int radius_squared = dx * dx + dy * dy;
    /* Easier to trace than the old Manhattan threshold, with hysteresis to
     * ignore neutral-stick noise that could otherwise start a false gesture. */
    if (radius_squared < 55 * 55) {
        previous_sector = -1;
        progress = 0;
        wait_for_center = false;
        gesture_active = false;
        return;
    }
    if (!gesture_active) {
        if (radius_squared < 75 * 75) return;
        gesture_active = true;
    }
    if (wait_for_center) return;

    // Eight sectors clockwise from right. A full circle needs eight
    // neighboring transitions in one direction without reversing.
    int sector;
    if (ax > ay * 2) sector = dx > 0 ? 0 : 4;
    else if (ay > ax * 2) sector = dy < 0 ? 2 : 6;
    else if (dx > 0) sector = dy < 0 ? 1 : 7;
    else sector = dy < 0 ? 3 : 5;
    if (previous_sector < 0) {
        previous_sector = sector;
        gesture_start_ms = last_step_ms = now;
        return;
    }
    if (sector == previous_sector) return;
    int delta = (sector - previous_sector + 8) % 8;
    previous_sector = sector;
    /* Accept a skipped octant when a quick movement crosses between samples. */
    if (delta != 1 && delta != 2 && delta != 6 && delta != 7) {
        progress = 0;
        gesture_start_ms = last_step_ms = now;
        return;
    }
    int step = delta <= 2 ? delta : delta - 8;
    if (now - last_step_ms > 900) {
        progress = 0;
        gesture_start_ms = now;
    }
    progress += step;
    last_step_ms = now;
    if (abs(progress) >= 8) {
        if (now - gesture_start_ms >= 200 && now - gesture_start_ms <= 6000) {
            bool lock = progress > 0;
            if (track_keys_locked != lock) {
                track_keys_locked = lock;
                playback_status = lock ? "L/R verrouilles" : "L/R deverrouilles";
                ui_dirty = true;
            }
        }
        wait_for_center = true;
    }
}

static void save_brightness_for_home(void) {
    // Keep the user's chosen level as the system preset so HOME does not
    // restore its previous level (or flash to the default) when we exit.
    // Preserve byte 0, which stores the separate power-saving setting.
    if (!brightness_changed || !backlight_config_valid || !cfgu_service_ready)
        return;
    backlight_config[1] = (u8)brightness_level;
    if (R_SUCCEEDED(CFG_SetConfigInfoBlk4(sizeof(backlight_config), 0x00050001,
                                          backlight_config)))
        (void)CFG_UpdateConfigSavegame();
}

int main(void) {
    gfxInitDefault();
    hidScanInput();
    force_language_selection = (hidKeysHeld() & KEY_X) != 0;
    aptSetHomeAllowed(false);
    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    show_boot_splash();
    consoleInit(GFX_TOP, &top_console);
    consoleInit(GFX_BOTTOM, &bottom_console);
    consoleSetWindow(&top_console, 0, 0, top_console.consoleWidth, top_console.consoleHeight);
    consoleSetWindow(&bottom_console, 0, 0, bottom_console.consoleWidth, bottom_console.consoleHeight);
    consoleSelect(&top_console);
    fs_service_ready = R_SUCCEEDED(fsInit());
    update_sd_storage();
    ptmu_service_ready = R_SUCCEEDED(ptmuInit());
    update_charging_state();
    battery_service_ready = R_SUCCEEDED(mcuHwcInit());
    update_battery();
    battery_last_update = osGetTime();
    charger_last_update = battery_last_update;
    update_battery_temperature();
    temperature_last_update = battery_last_update;

    cfgu_service_ready = R_SUCCEEDED(cfguInit());
    if (cfgu_service_ready) {
        u8 system_model;
        if (R_SUCCEEDED(CFGU_GetSystemModel(&system_model)) &&
            system_model == CFG_MODEL_2DS) {
            thermal_warning_threshold = 42;
        }
        if (R_SUCCEEDED(CFGU_GetConfigInfoBlk2(sizeof(backlight_config), 0x00050001,
                                               backlight_config)) &&
            backlight_config[1] >= 1 && backlight_config[1] <= 5) {
            brightness_level = backlight_config[1];
            backlight_config_valid = true;
        }
    }
    bool language_prompt_was_requested = force_language_selection;
    choose_ui_language(force_language_selection);
    if (fs_service_ready) show_first_launch_guide(language_prompt_was_requested);
    lcd_ready = R_SUCCEEDED(gspLcdInit());

    Result audio_result = ndspInit();
    audio_ready = R_SUCCEEDED(audio_result);
    if (audio_ready) {
        for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
            blocks[i].pcm = (int16_t *)linearAlloc(BLOCK_FRAMES * 2 * sizeof(int16_t));
            if (!blocks[i].pcm) audio_ready = false;
        }
    }
    if (audio_ready) {
        ndspSetMasterVol(1.0f);
        ndspSetClippingMode(NDSP_CLIP_SOFT);
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspChnReset(CHANNEL);
        // Let the DSP perform its higher-quality polyphase resampling when
        // pitch and playback speed change; keep ARM11 free for decoding.
        ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
        ndspChnSetFormat(CHANNEL, NDSP_FORMAT_STEREO_PCM16);
        float mix[12] = {0};
        mix[0] = mix[1] = 1.0f;
        ndspChnSetMix(CHANNEL, mix);
        apply_tone();
        ndspChnSetPaused(CHANNEL, false);
    } else {
        audio_status = R_FAILED(audio_result) ? "dspfirm.cdc absent dans sdmc:/3ds/" : "memoire audio insuffisante";
        for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
            if (blocks[i].pcm) { linearFree(blocks[i].pcm); blocks[i].pcm = NULL; }
        }
        if (R_SUCCEEDED(audio_result)) ndspExit();
    }
    srand((unsigned)osGetTime());
    scan_folder();
    update_clock(osGetTime());
    draw();
    (void)appWifiGuardInit(&wifi_guard);

    int pitch_held_direction = 0;
    u64 pitch_next_repeat_at = 0;
    const u32 joystick_keys = KEY_CPAD_UP | KEY_CPAD_DOWN | KEY_CPAD_LEFT | KEY_CPAD_RIGHT |
                              KEY_CSTICK_UP | KEY_CSTICK_DOWN | KEY_CSTICK_LEFT | KEY_CSTICK_RIGHT;
    for (;;) {
        // Sleep remains enabled for lid-close playback policy. HOME stays
        // blocked; POWER is left entirely to the system.
        aptHandleSleep();
        if (aptShouldClose()) break;
        u64 now = osGetTime();
        update_clock(now);
        update_playback_clock(now);
        if (now - battery_last_update >= 5000) {
            float old_battery = battery_percent;
            update_battery();
            battery_last_update = now;
            if (battery_percent != old_battery) ui_dirty = true;
        }
        if (now - charger_last_update >= 1000) {
            bool old_known = charging_state_known;
            bool old_charging = battery_charging;
            update_charging_state();
            charger_last_update = now;
            if (charging_state_known != old_known || battery_charging != old_charging)
                ui_dirty = true;
        }
        if (now - temperature_last_update >= 15000) {
            int old_temperature = battery_temperature_c;
            update_battery_temperature();
            temperature_last_update = now;
            if (battery_temperature_c != old_temperature) ui_dirty = true;
        }
        hidScanInput();
        if (!options_open) update_track_key_lock(now);
        u32 down = hidKeysDown() & ~joystick_keys;
        u32 held = hidKeysHeld() & ~joystick_keys;
        u32 released = hidKeysUp() & ~joystick_keys;
        if (aptCheckHomePressRejected() && now - last_home_toggle_ms >= 350) {
            audio_boost_mode = (audio_boost_mode + 1) % 4;
            last_home_toggle_ms = now;
            playback_status = audio_boost_mode == 3 ? "Boost audio 500%" :
                              audio_boost_mode == 2 ? "Boost audio 300%" :
                              audio_boost_mode == 1 ? "Boost audio 200%" :
                                                      "Boost audio desactive";
            ui_dirty = true;
        }
        if (down) ui_dirty = true;
        bool menu_handled = false;
        if (down & KEY_SELECT) {
            options_open = !options_open;
            options_page = options_selection = 0;
            touch_seek_dragging = false;
            touch_reverb_dragging = false;
            ui_dirty = true;
            menu_handled = true;
        } else if (options_open) {
            menu_handled = true;
            if (down & KEY_B) {
                if (options_page == 0) options_open = false;
                else { options_page = 0; options_selection = 0; }
            } else if (down & KEY_DUP) {
                if (options_selection) options_selection--;
            } else if (down & KEY_DDOWN) {
                unsigned last = options_page == 0 ? 1 : options_page == 1 ? 6 : 1;
                if (options_selection < last) options_selection++;
            } else if (down & KEY_A) options_activate();
            else if (down & KEY_TOUCH) {
                touchPosition touch;
                hidTouchRead(&touch);
                unsigned row = touch.py / 8;
                if (options_page == 0) {
                    if (row >= 7 && row <= 9) { options_selection = 0; options_activate(); }
                    else if (row >= 11 && row <= 13) { options_selection = 1; options_activate(); }
                } else if (options_page == 1) {
                    if (row >= 6 && row <= 25) {
                        options_selection = (row - 6) / 3;
                        if (options_selection > 6) options_selection = 6;
                        options_activate();
                    }
                } else if (row >= 9 && row <= 11) {
                    options_selection = 0; options_activate();
                } else if (row >= 13 && row <= 15) {
                    options_selection = 1; options_activate();
                }
            }
        }
        if (menu_handled) down = held = released = 0;
        if (down & KEY_START) break;
        if (playing && (down & KEY_TOUCH)) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (touch.py >= BOTTOM_REVERB_LABEL_ROW * 8 - 3 &&
                touch.py < BOTTOM_REVERB_ROW * 8 + 25) {
                touch_reverb_dragging = true;
                touch_reverb_preview = reverb_touch_value(touch.px);
                draw_reverb_bar();
            } else if (touch.py >= BOTTOM_PROGRESS_ROW * 8 - 3 &&
                       touch.py < BOTTOM_PROGRESS_ROW * 8 + 13) {
                if (!track_duration_ms) {
                    playback_status = "Duree inconnue : deplacement indisponible.";
                    ui_dirty = true;
                } else {
                    touch_seek_dragging = true;
                    touch_seek_target_ms = touch_target_ms(touch.px);
                    last_touch_preview_ms = now;
                    draw_progress_bar();
                }
            }
        }
        if (touch_reverb_dragging && (held & KEY_TOUCH)) {
            touchPosition touch;
            hidTouchRead(&touch);
            unsigned preview = reverb_touch_value(touch.px);
            if (preview != touch_reverb_preview) {
                touch_reverb_preview = preview;
                draw_reverb_bar();
            }
        }
        if (touch_reverb_dragging && (released & KEY_TOUCH)) {
            if (reverb_custom_enabled) reverb_custom_percent = touch_reverb_preview;
            else reverb_mode = touch_reverb_preview;
            touch_reverb_dragging = false;
            ui_dirty = true;
        }
        if (touch_seek_dragging && (held & KEY_TOUCH)) {
            touchPosition touch;
            hidTouchRead(&touch);
            u64 target = touch_target_ms(touch.px);
            if (target != touch_seek_target_ms) {
                touch_seek_target_ms = target;
                if (now - last_touch_preview_ms >= 75) {
                    draw_progress_bar();
                    last_touch_preview_ms = now;
                }
            }
        }
        if (touch_seek_dragging && (released & KEY_TOUCH)) {
            u64 target = touch_seek_target_ms;
            touch_seek_dragging = false;
            if (now - last_touch_seek_ms >= 450) {
                last_touch_seek_ms = now;
                seek_track(target);
            }
            ui_dirty = true;
        }
        if (down & KEY_A) {
            if (!playing && entry_count && entries[selection].directory) enter_selected_directory();
            else if (!playing && entry_count && !entries[selection].directory) {
                build_order(-1);
                for (int i = 0; i < track_count; i++)
                    if (!strcmp(tracks[order[i]].name, entries[selection].name)) { open_track(i); break; }
            }
            else if (playing && audio_ready) {
                paused = !paused;
                ndspChnSetPaused(CHANNEL, paused);
                aptSetSleepAllowed(paused);
            }
        }
        if (down & KEY_B) { if (playing) stop_track(true); else parent_directory(); }
        if (playing && (down & KEY_X)) {
            repeat_current = !repeat_current;
            playback_status = repeat_current ? "Repetition de la piste activee" :
                                               "Repetition de la piste desactivee";
            ui_dirty = true;
        }
        if (down & KEY_Y) {
            if (playing && audio_ready) {
                bass_profile = (bass_profile + 1) % 5;
                apply_tone();
                ui_dirty = true;
                playback_status = bass_profile == 4 ? "Basses EXTREME (+22 dB)" :
                                  bass_profile == 3 ? "Basses +16 dB" :
                                  bass_profile == 2 ? "Basses BOOST (+9 dB)" :
                                  bass_profile == 1 ? "Basses DOUX (+6 dB)" :
                                                      "Basses OFF";
            } else {
                start_folder(-1);
            }
        }
        if (playing && !track_keys_locked && (down & KEY_L)) step_track(-1);
        if (playing && !track_keys_locked && (down & KEY_R)) step_track(1);
        if (down & KEY_DUP) {
            if (playing) change_brightness(1);
            else if (selection > 0) selection--;
        }
        if (down & KEY_DDOWN) {
            if (playing) change_brightness(-1);
            else if (selection + 1 < entry_count) selection++;
        }
        // Only the D-pad changes pitch. One tap is one 0.025x step;
        // holding repeats after 350 ms, then every 160 ms until release.
        int pitch_direction = ((held & KEY_DRIGHT) ? 1 : 0) - ((held & KEY_DLEFT) ? 1 : 0);
        if (!playing || !audio_ready || pitch_direction == 0) {
            pitch_held_direction = 0;
            pitch_next_repeat_at = 0;
        } else {
            bool first_step = pitch_direction != pitch_held_direction;
            if (first_step || now >= pitch_next_repeat_at) {
                int next = pitch_milli + pitch_direction * 25;
                if (next < 250) next = 250;
                if (next > 2000) next = 2000;
                if (next != pitch_milli) {
                    pitch_milli = next;
                    speed = pitch_milli / 1000.0f;
                    update_output_rate();
                    pitch_dirty = true;
                }
                pitch_next_repeat_at = now + (first_step ? 450 : 200);
            }
            pitch_held_direction = pitch_direction;
        }
        if (playing && audio_ready) advance_output_rate();
        if (playing && !paused && audio_file) {
            for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
                if (!audio_eof && blocks[i].wave.status == NDSP_WBUF_DONE)
                    queue_block(&blocks[i]);
            }
            bool drained = true;
            for (int i = 0; i < AUDIO_BLOCK_COUNT; i++)
                if (blocks[i].wave.status == NDSP_WBUF_QUEUED ||
                    blocks[i].wave.status == NDSP_WBUF_PLAYING) drained = false;
            if (audio_eof && drained) {
                if (!track_has_audio) {
                    failed_tracks_in_a_row++;
                    if (failed_tracks_in_a_row >= track_count) {
                        stop_track(true);
                        playback_status = "Aucun MP3 ou OGG lisible dans ce dossier.";
                    } else {
                        step_track(1);
                    }
                } else {
                    failed_tracks_in_a_row = 0;
                    if (!repeat_current || !open_track(order_pos))
                        step_track(1);
                }
                ui_dirty = true;
            }
        }
        (void)held;
        if (ui_dirty) draw();
        else {
            if (pitch_dirty) draw_pitch_value();
            if (time_dirty) {
                draw_playback_time();
                if (!options_open) draw_progress_bar();
            }
        }
        // VBlank can stop while the lid is closed. Do not make audio refill
        // and next-track handling depend on a display event.
        svcSleepThread(5 * 1000 * 1000LL);
    }

    save_brightness_for_home();
    stop_track(true);
    if (audio_ready) ndspExit();
    appWifiGuardExit(&wifi_guard);
    if (fs_service_ready) fsExit();
    if (ptmu_service_ready) ptmuExit();
    if (battery_service_ready) mcuHwcExit();
    if (cfgu_service_ready) cfguExit();
    if (lcd_ready) {
        gspLcdExit();
    }
    for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
        if (blocks[i].pcm) linearFree(blocks[i].pcm);
    }
    gfxExit();
    return 0;
}
