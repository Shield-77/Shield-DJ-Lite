#ifndef SHIELD_UI_LANGUAGE_H
#define SHIELD_UI_LANGUAGE_H

#include <3ds.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    bool french;
    bool confirmation_needed;
    const char *config_path;
} ShieldUiLanguage;

static ShieldUiLanguage shield_ui_language;

typedef struct { const char *fr; const char *en; } ShieldUiTranslation;

/* Keep this table limited to interface text. File names and music metadata
 * are deliberately passed through unchanged. */
static const ShieldUiTranslation shield_ui_translations[] = {
    {"TA MUSIQUE", "YOUR MUSIC"},
    {"   Mets tes MP3 ou OGG/Vorbis dans un dossier\n", "   Put MP3 or OGG/Vorbis files in a folder\n"},
    {"   de la carte SD.\n", "   on the SD card.\n"},
    {"   MP3 et OGG/Vorbis seulement.\n", "   MP3 and OGG/Vorbis only.\n"},
    {"   Lite est legere ; Standard est plus soignee.\n", "   Lite is lighter; Standard is more polished.\n"},
    {"Choisir un fichier", "Choose a file"},
    {"Ouvrir ou lire", "Open or play"},
    {"Dossier precedent", "Previous folder"},
    {"Lire le dossier en aleatoire", "Shuffle this folder"},
    {"PENDANT LA LECTURE", "DURING PLAYBACK"},
    {"Pause / reprendre", "Pause / resume"},
    {"Arreter", "Stop"},
    {"Morceau precedent / suivant", "Previous / next track"},
    {"Pitch et vitesse", "Pitch and speed"},
    {"Luminosite", "Brightness"},
    {"   Barre tactile : glisser et relacher.\n", "   Touch bar: slide, then release.\n"},
    {"\n   Circle Pad gauche : tour droite bloque L/R.\n", "\n   Left Circle Pad: turn clockwise to lock L/R.\n"},
    {"   Si besoin, fais un 2e tour; geste en dev.\n", "   If needed, try 2 turns; still in development.\n"},
    {"   Tour gauche : debloque; recentre le pad.\n", "   Turn counter-clockwise to unlock; then recenter.\n"},
    {"REGLAGES ET AIDE", "SETTINGS AND HELP"},
    {"Repeter le morceau", "Repeat track"},
    {"Changer les basses", "Change bass"},
    {"Changer la reverb", "Change reverb"},
    {"Ouvrir / fermer les options", "Open / close options"},
    {"Boost OFF / 2x / 3x / 5x", "Boost OFF / 2x / 3x / 5x"},
    {"Quitter l'application", "Quit the app"},
    {"\n   Pas de son ? Lance DSP1 une fois.\n", "\n   No sound? Run DSP1 once.\n"},
    {"   SELECT au boot : revoir ce guide.\n", "   Hold SELECT at startup to replay this guide.\n"},
    {"UN PROJET BENEVOLE", "A VOLUNTEER PROJECT"},
    {"   Je developpe Shield DJ Lite\n", "   I build Shield DJ Lite\n"},
    {"   gratuitement, sur mon temps libre,\n", "   for free, in my spare time,\n"},
    {"   avec des moyens limites.\n\n", "   with limited resources.\n\n"},
    {"   Je fais de mon mieux pour rendre\n", "   I do my best to make it\n"},
    {"   l'application stable et agreable.\n\n", "   stable and easy to use.\n\n"},
    {"   Des bugs restent possibles :\n", "   Bugs may still occur:\n"},
    {"   je m'en excuse et continuerai\n", "   I'm sorry; I will keep\n"},
    {"   a les corriger. Merci !\n", "   working to fix them. Thanks!\n"},
    {"A  PAGE SUIVANTE", "A  NEXT PAGE"},
    {"B  RETOUR       ", "B  BACK         "},
    {"A  J'AI COMPRIS", "A  GOT IT"},
    {"SHIELD DJ LITE BY SHIELD-77", "SHIELD DJ LITE BY SHIELD-77"},
    {"Batterie : %d%%\n", "Battery: %d%%\n"},
    {"Batterie : indisponible\n", "Battery: unavailable\n"},
    {"BATTERIE FAIBLE - recharge la console", "LOW BATTERY - please recharge"},
    {"Lecteur audio MP3 / OGG Vorbis\n", "MP3 / OGG Vorbis audio player\n"},
    {"DOSSIER", "FOLDER"},
    {"SON INDISPONIBLE", "AUDIO UNAVAILABLE"},
    {"Cause : %s\n", "Reason: %s\n"},
    {"Lance DSP1 depuis Homebrew Launcher, puis\n", "Run DSP1 from the Homebrew Launcher, then\n"},
    {"relance Shield DJ Lite by Shield-77.\n\n", "restart Shield DJ Lite by Shield-77.\n\n"},
    {"AUDIO PRET", "AUDIO READY"},
    {"Etat : %s\n", "Status: %s\n"},
    {"En pause", "Paused"}, {"Lecture", "Playing"}, {"A l'arret", "Stopped"},
    {"Piste : ", "Track: "},
    {"Temps : ", "Time: "},
    {"Pitch + vitesse : %s   ", "Pitch + speed: %s   "},
    {"REGLAGES", "SETTINGS"},
    {"Pitch + vitesse : %s\n", "Pitch + speed: %s\n"},
    {"Profil basses : %s\n", "Bass profile: %s\n"},
    {"EXTREME +22 dB", "EXTREME +22 dB"}, {"BOOST +9 dB", "BOOST +9 dB"},
    {"DOUX +6 dB", "SOFT +6 dB"}, {"NON", "OFF"},
    {"Reverb : %s\n", "Reverb: %s\n"},
    {"CONCERT", "CONCERT"}, {"CATHEDRALE", "CATHEDRAL"}, {"EXTRA", "EXTRA"},
    {"Aucun", "None"}, {"Rock", "Rock"}, {"Pop", "Pop"},
    {"Jazz", "Jazz"}, {"Clair", "Clear"}, {"Exterieur", "Outdoor"},
    {"Classique", "Classic"},
    {"Luminosite : %d/5\n", "Brightness: %d/5\n"},
    {"Shuffle : %s\n", "Shuffle: %s\n"},
    {"active", "ON"}, {"desactive", "OFF"},
    {"Touches L/R : %s\n", "L/R buttons: %s\n"},
    {"BLOQUEES", "LOCKED"}, {"ACTIVES", "UNLOCKED"},
    {"Repeter piste : %s (X)\n", "Repeat track: %s (X)\n"},
    {"OUI", "ON"}, {"NON", "OFF"},
    {"Boost son : %s (HOME)\n", "Audio boost: %s (HOME)\n"},
    {"SD : %llu.%02llu Go libres / %llu.%02llu Go", "SD: %llu.%02llu GB free / %llu.%02llu GB"},
    {"SD : espace indisponible", "SD: storage unavailable"},
    {"SURCHAUFFE THERMIQUE EN COURS", "OVERHEATING - STOP PLAYBACK"},
    {"BIBLIOTHEQUE", "LIBRARY"},
    {"Fichiers audio dans ce dossier : %d\n\n", "Audio files in this folder: %d\n\n"},
    {"Contenu :\n", "Contents:\n"},
    {"A      Pause / reprendre\n", "A      Pause / resume\n"},
    {"B      Arreter\n", "B      Stop\n"},
    {"L / R  Piste precedente / suivante\n", "L / R  Previous / next track\n"},
    {"X      Repeter piste oui/non\n", "X      Repeat track on/off\n"},
    {"Gauche/Droite  Pitch et vitesse\n", "Left/Right  Pitch and speed\n"},
    {"Haut/Bas       Luminosite\n", "Up/Down       Brightness\n"},
    {"Y              Profil basses suivant\n", "Y              Next bass profile\n"},
    {"SELECT         Reverb suivante\n", "SELECT         Next reverb\n"},
    {"HOME           Boost OFF/200/300/500\n", "HOME           Boost OFF/200/300/500\n"},
    {"\nTrames audio decodees : %lu\n", "\nAudio frames decoded: %lu\n"},
    {"Trames audio perdues : %lu\n", "Audio frames dropped: %lu\n"},
    {"\nHaut/Bas choisir | A ouvrir/lire\n", "\nUp/Down select | A open/play\n"},
    {"B parent | Y lire dossier\n", "B parent | Y shuffle folder\n"},
    {"START quitter\n", "START quit\n"}, {"START quitter", "START quit"},
    {"Glisser sur la barre, puis relacher", "Slide the bar, then release"},
    {"Temp. batt. : %d deg C", "Battery temp: %d deg C"},
    {"Temp. batt. : indisponible", "Battery temp: unavailable"},
    {"Impossible d'ouvrir ce dossier SD.", "Could not open this SD folder."},
    {"Impossible d'ouvrir ce fichier audio.", "Could not open this audio file."},
    {"Fichier OGG/Vorbis invalide ou non pris en charge.", "Invalid or unsupported OGG/Vorbis file."},
    {"OGG invalide : mono/stereo et 8-48 kHz requis.", "Invalid OGG: mono/stereo and 8-48 kHz required."},
    {"En-tete MP3 invalide ou trop grand.", "Invalid or oversized MP3 header."},
    {"Cet OGG ne permet pas ce deplacement.", "Seeking is unavailable for this OGG."},
    {"Impossible de se deplacer dans ce MP3.", "Could not seek within this MP3."},
    {"Position modifiee.", "Position changed."},
    {"Aucun MP3 ou OGG dans ce dossier.", "No MP3 or OGG files in this folder."},
    {"dspfirm.cdc absent dans sdmc:/3ds/", "dspfirm.cdc is missing from sdmc:/3ds/"},
    {"memoire audio insuffisante", "Not enough audio memory"},
    {"Boost audio desactive", "Audio boost disabled"},
    {"Repetition de la piste activee", "Track repeat enabled"},
    {"Repetition de la piste desactivee", "Track repeat disabled"},
    {"Basses +16 dB", "Bass +16 dB"},
    {"Basses OFF", "Bass OFF"},
    {"Duree inconnue : deplacement indisponible.", "Unknown duration: seeking is unavailable."},
    {"Basses EXTREME (+22 dB)", "EXTREME bass (+22 dB)"},
    {"Basses BOOST (+9 dB)", "BOOST bass (+9 dB)"},
    {"Basses DOUX (+6 dB)", "SOFT bass (+6 dB)"},
    {"Profil de reverb modifie", "Reverb preset changed"},
    {"Reverb desactivee", "Reverb disabled"},
    {"Aucun MP3 ou OGG lisible dans ce dossier.", "No playable MP3 or OGG files in this folder."},
    {"L/R verrouilles", "L/R locked"}, {"L/R deverrouilles", "L/R unlocked"},
};

static inline const char *shield_ui_translate(const char *text) {
    if (!text || shield_ui_language.french) return text;
    for (size_t i = 0; i < sizeof(shield_ui_translations) / sizeof(shield_ui_translations[0]); ++i)
        if (strcmp(text, shield_ui_translations[i].fr) == 0)
            return shield_ui_translations[i].en;
    return text;
}

static inline const char *shield_ui_translate_format(const char *format,
                                                       char *out, size_t cap) {
    if (!format || !out || !cap || shield_ui_language.french) return format;
    size_t used = 0;
    const char *at = format;
    while (*at && used + 1 < cap) {
        if ((unsigned char)*at == 0x1b) {
            do {
                if (used + 1 >= cap) break;
                out[used++] = *at;
            } while (*at++ && *(at - 1) != 'm');
            continue;
        }
        const char *end = at;
        while (*end && (unsigned char)*end != 0x1b) end++;
        size_t len = (size_t)(end - at);
        char segment[512];
        if (len >= sizeof(segment)) len = sizeof(segment) - 1;
        memcpy(segment, at, len); segment[len] = 0;
        const char *translated = shield_ui_translate(segment);
        size_t add = strlen(translated);
        if (add > cap - used - 1) add = cap - used - 1;
        memcpy(out + used, translated, add); used += add;
        at = end;
    }
    out[used] = 0;
    return out;
}

static inline int shield_ui_printf(const char *format, ...) {
    char translated[1024];
    const char *use = shield_ui_translate_format(format, translated, sizeof(translated));
    va_list args;
    va_start(args, format);
    int result = vprintf(use, args);
    va_end(args);
    return result;
}

static inline void shield_ui_language_load(ShieldUiLanguage *state,
                                           const char *config_path,
                                           bool cfgu_already_ready) {
    if (!state) return;
    state->french = false;
    state->confirmation_needed = false;
    state->config_path = config_path;
    FILE *file = config_path ? fopen(config_path, "rb") : NULL;
    if (file) {
        int saved = fgetc(file);
        fclose(file);
        if (saved == 'F' || saved == 'E') {
            state->french = saved == 'F';
            return;
        }
    }
    u8 system_language = CFG_LANGUAGE_EN;
    bool close_cfgu = false;
    bool language_read = false;
    if (cfgu_already_ready) language_read = R_SUCCEEDED(CFGU_GetSystemLanguage(&system_language));
    else if (R_SUCCEEDED(cfguInit())) {
        close_cfgu = true;
        language_read = R_SUCCEEDED(CFGU_GetSystemLanguage(&system_language));
    }
    if (close_cfgu) cfguExit();
    state->french = language_read && system_language == CFG_LANGUAGE_FR;
    state->confirmation_needed = true;
}

static inline bool shield_ui_language_save(const ShieldUiLanguage *state) {
    if (!state || !state->config_path) return false;
    (void)mkdir("sdmc:/3ds", 0777);
    char parent[160];
    size_t length = strlen(state->config_path);
    if (length >= sizeof(parent)) return false;
    memcpy(parent, state->config_path, length + 1);
    char *slash = strrchr(parent, '/');
    if (slash && slash > parent + 5) { *slash = 0; (void)mkdir(parent, 0777); }
    FILE *file = fopen(state->config_path, "wb");
    if (!file) return false;
    char value = state->french ? 'F' : 'E';
    bool okay = fwrite(&value, 1, 1, file) == 1;
    if (fclose(file) != 0) okay = false;
    return okay;
}

static inline void shield_ui_language_choose(ShieldUiLanguage *state,
                                              bool accept_detected_language) {
    if (!state) return;
    if (!accept_detected_language) state->french = !state->french;
    state->confirmation_needed = false;
    (void)shield_ui_language_save(state);
}

static inline void shield_ui_language_set(ShieldUiLanguage *state, bool french) {
    if (!state) return;
    state->french = french;
    state->confirmation_needed = false;
    (void)shield_ui_language_save(state);
}

#endif
