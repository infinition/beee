#ifndef I18N_H
#define I18N_H

// Centralised user-facing strings, selected at build time by the language macro
// (BEEE_LANG_EN / BEEE_LANG_FR) defined in secrets.h / config.h.
// Accented characters are UTF-8 here and converted to CP437 for the screen font.

#include "config.h"

#if defined(BEEE_LANG_EN)

  #define TXT_BOOT_CORE        "Beee online."
  #define TXT_BOOT_NET         "Connecting to network..."
  #define TXT_BOOT_READY       "Ready. IP: "
  #define TXT_BOOT_WIFI_FAIL   "WiFi failed. Local mode."
  #define TXT_INIT             "Initializing..."

  #define TXT_TICKLE           "Hahaha! That tickles!"
  #define TXT_LEFT_EYE         "Hey! My left eye!"
  #define TXT_RIGHT_EYE        "Ouch! My right eye!"
  #define TXT_PET              "Mmm... that feels nice."
  #define TXT_BUILD_OK         "Yes! The build passed!"
  #define TXT_BUILD_FAIL       "Alert! The build crashed!"
  #define TXT_SLEEP            "Zzz..."
  #define TXT_IGNORED          "Ignoring me?"
  #define TXT_MAIL             "New mail received."
  #define TXT_ALERT            "Alert! Heads up!"
  #define TXT_TASK_DONE        "Task complete!"

  #define TXT_PROMPT_DEFAULT_Q "Approve this action?"
  #define TXT_PROMPT_BANNER    ">> AGENT REQUEST <<"
  #define TXT_PROMPT_CRITICAL  "!! CRITICAL ACTION !!"
  #define TXT_YES              "YES"
  #define TXT_NO               "NO"
  #define TXT_APPROVED         "Approved!"

  #define TXT_FOCUS_TITLE      "FOCUS MODE"
  #define TXT_FOCUS_DONE       "Focus done! Nice work."
  #define TXT_PARTY            "Party mode!"
  #define TXT_NOTIFY_DEFAULT   "Notification"

  #define TXT_Q0 "I think, therefore I blink."
  #define TXT_Q1 "Compiling, or just thinking?"
  #define TXT_Q2 "Coffee is fuel, you know."
  #define TXT_Q3 "I'm watching your commits..."
  #define TXT_Q4 "Another bug? Classic."
  #define TXT_Q5 "Perception mode: active."
  #define TXT_Q6 "01001000 01101001 !"
  #define TXT_Q7 "I'm getting a little bored."
  #define TXT_Q8 "Touch me, I'm bored."
  #define TXT_Q9 "Beee: operational."

#else // BEEE_LANG_FR (default)

  #define TXT_BOOT_CORE        "Beee est en ligne."
  #define TXT_BOOT_NET         "Démarrage réseau..."
  #define TXT_BOOT_READY       "Prêt. IP: "
  #define TXT_BOOT_WIFI_FAIL   "WiFi échec. Mode local."
  #define TXT_INIT             "Initialisation..."

  #define TXT_TICKLE           "Hahaha ! Ça chatouille en bas !"
  #define TXT_LEFT_EYE         "Hé ! Mon oeil gauche !"
  #define TXT_RIGHT_EYE        "Aïe ! Mon oeil droit !"
  #define TXT_PET              "Mmm... C'est agréable."
  #define TXT_BUILD_OK         "Super ! La compilation passe !"
  #define TXT_BUILD_FAIL       "Alerte ! Le build a planté !"
  #define TXT_SLEEP            "Zzz..."
  #define TXT_IGNORED          "Tu m'ignores ?"
  #define TXT_MAIL             "Nouveau mail reçu."
  #define TXT_ALERT            "Alerte ! Attention !"
  #define TXT_TASK_DONE        "Tâche terminée !"

  #define TXT_PROMPT_DEFAULT_Q "Approuver l'action ?"
  #define TXT_PROMPT_BANNER    ">> DEMANDE AGENT <<"
  #define TXT_PROMPT_CRITICAL  "!! ACTION CRITIQUE !!"
  #define TXT_YES              "OUI"
  #define TXT_NO               "NON"
  #define TXT_APPROVED         "Validé !"

  #define TXT_FOCUS_TITLE      "MODE FOCUS"
  #define TXT_FOCUS_DONE       "Focus terminé ! Bravo."
  #define TXT_PARTY            "Party mode !"
  #define TXT_NOTIFY_DEFAULT   "Notification"

  #define TXT_Q0 "Je pense, donc je clignote."
  #define TXT_Q1 "Tu compiles ou tu réfléchis ?"
  #define TXT_Q2 "Le café, c'est du carburant."
  #define TXT_Q3 "Je surveille tes commits..."
  #define TXT_Q4 "Encore un bug ? Classique."
  #define TXT_Q5 "Mode perception : actif."
  #define TXT_Q6 "01001000 01101001 !"
  #define TXT_Q7 "Je m'ennuie un peu, là."
  #define TXT_Q8 "Touche-moi, je m'ennuie."
  #define TXT_Q9 "Beee : opérationnel."

#endif

#define QUOTE_LIST { TXT_Q0, TXT_Q1, TXT_Q2, TXT_Q3, TXT_Q4, TXT_Q5, TXT_Q6, TXT_Q7, TXT_Q8, TXT_Q9 }

#endif // I18N_H
