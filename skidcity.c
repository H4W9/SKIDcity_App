/*
 * SKIDcity — Flipper Zero Educational App
 * "Don't be a SKID!"
 *
 * Every fake "hack" on the main menu either runs a harmless
 * Flipper demo or shows legal/educational information about
 * why that action is illegal or impossible.
 */

#include "skidcity.h"
#include <stdlib.h>
#include <string.h>

#define SKIDCITY_VERSION "1.0"

/* Scrolling header: full string rotates through a display window */
#define HEADER_STR  "  SKIDcity v" SKIDCITY_VERSION "  ~  Don't Be A SKID!  "
#define HEADER_WIN  22  /* characters visible at once */

/* ── Forward-declare all scene handlers via X-macro ── */
#define ADD_SCENE(module, name, id) \
    void module##_scene_##name##_on_enter(void*); \
    bool module##_scene_##name##_on_event(void*, SceneManagerEvent); \
    void module##_scene_##name##_on_exit(void*);
#include "skidcity_scene_config.h"
#undef ADD_SCENE

/* ── Scene handler tables ── */
static void (*const skidcity_on_enter_handlers[])(void*) = {
#define ADD_SCENE(module, name, id) module##_scene_##name##_on_enter,
#include "skidcity_scene_config.h"
#undef ADD_SCENE
};
static bool (*const skidcity_on_event_handlers[])(void*, SceneManagerEvent) = {
#define ADD_SCENE(module, name, id) module##_scene_##name##_on_event,
#include "skidcity_scene_config.h"
#undef ADD_SCENE
};
static void (*const skidcity_on_exit_handlers[])(void*) = {
#define ADD_SCENE(module, name, id) module##_scene_##name##_on_exit,
#include "skidcity_scene_config.h"
#undef ADD_SCENE
};
static const SceneManagerHandlers skidcity_scene_handlers = {
    .on_enter_handlers = skidcity_on_enter_handlers,
    .on_event_handlers = skidcity_on_event_handlers,
    .on_exit_handlers  = skidcity_on_exit_handlers,
    .scene_num         = SkidCitySceneCount,
};

/* ═══════════════════════════════════════════════════════
 * LED NOTIFICATION SEQUENCES
 * ═══════════════════════════════════════════════════════ */
/* ═══════════════════════════════════════════════════════
 * IR DEMO — NEC signal: Samsung TV Power (addr=0x07 cmd=0x02)
 * Pre-computed alternating mark/space durations in µs.
 * Even indices = mark (carrier on), odd = space (carrier off).
 * ═══════════════════════════════════════════════════════ */
static const uint32_t skidcity_ir_signal[] = {
    /* Leader */
    9000, 4500,
    /* Address 0x07 = 0000 0111 (LSB first) */
    560,560,  560,560,  560,560,  560,1690,  560,1690,  560,1690,  560,560,  560,560,
    /* ~Address 0xF8 = 1111 1000 (LSB first) */
    560,1690, 560,1690, 560,1690, 560,560,   560,560,   560,560,   560,1690, 560,1690,
    /* Command 0x02 = 0000 0010 (LSB first) */
    560,560,  560,1690, 560,560,  560,560,   560,560,   560,560,   560,560,  560,560,
    /* ~Command 0xFD = 1111 1101 (LSB first) */
    560,1690, 560,560,  560,1690, 560,1690,  560,1690,  560,1690,  560,1690, 560,1690,
    /* Stop bit */
    560
};
static const size_t skidcity_ir_signal_len =
    sizeof(skidcity_ir_signal) / sizeof(skidcity_ir_signal[0]);

/* ISR state — must survive the async TX callback lifetime */
typedef struct {
    size_t   pos;
    bool     done;
} SkidCityIrTxState;
static SkidCityIrTxState skidcity_ir_tx_state;

static void skidcity_ir_isr_cb(void* ctx, bool* last, uint32_t* duration, bool* level) {
    UNUSED(ctx);
    SkidCityIrTxState* s = &skidcity_ir_tx_state;
    if(s->pos >= skidcity_ir_signal_len) {
        *last     = true;
        *duration = 0;
        *level    = false;
        s->done   = true;
        return;
    }
    *level    = (s->pos % 2 == 0); /* even = mark */
    *duration = skidcity_ir_signal[s->pos++];
    *last     = (s->pos >= skidcity_ir_signal_len);
}

/* ═══════════════════════════════════════════════════════
 * EDUCATIONAL TEXT CONTENT
 * ═══════════════════════════════════════════════════════ */
#define ABOUT_TRAFFIC \
    "Traffic lights use NTCIP\n" \
    "protocols on closed, wired\n" \
    "networks. NOT Sub-GHz.\n" \
    "NOT IR. NOT NFC. Period.\n\n" \
    "The Flipper has no NTCIP\n" \
    "stack and no way to reach\n" \
    "a wired traffic network.\n" \
    "It physically cannot talk\n" \
    "to a traffic controller.\n\n" \
    "The 'police strobe trick'\n" \
    "is an urban legend.\n\n" \
    "LEGAL USE: Study ITS specs\n" \
    "(NTCIP), get a career in\n" \
    "traffic engineering, or\n" \
    "volunteer for your city!"

#define ABOUT_WIFI \
    "18 U.S.C. S1030 - CFAA:\n" \
    "Unauthorized computer access\n" \
    "= up to 10 yrs federal prison\n" \
    "per offense + heavy fines.\n\n" \
    "Deauth, evil-twin, and packet\n" \
    "flooding attacks destroy\n" \
    "others' property.\n\n" \
    "The Flipper cannot do most\n" \
    "of these without an external\n" \
    "WiFi dev board anyway.\n\n" \
    "LEGAL: Use YOUR OWN network.\n" \
    "Earn CompTIA Sec+, CEH, OSCP."

#define ABOUT_CAR \
    "18 U.S.C. S2312 - Motor\n" \
    "Vehicle Theft Act.\n" \
    "State laws stack on top.\n\n" \
    "Modern key fobs use ROLLING\n" \
    "CODES (KeeLoq, AUT64).\n" \
    "Every press generates a new\n" \
    "one-time cryptographic code.\n\n" \
    "The Flipper CAN capture the\n" \
    "raw Sub-GHz signal. It CANNOT\n" \
    "break the rolling code crypto.\n" \
    "Replaying it = DOES NOTHING.\n\n" \
    "LEGAL: Research your OWN\n" \
    "vehicle RF system. Study\n" \
    "automotive cybersecurity."

#define ABOUT_CARDS \
    "18 U.S.C. S1029 - Access\n" \
    "Device Fraud: up to 15 yrs\n" \
    "+ $250,000 fine per count.\n\n" \
    "EMV chip cards use a new\n" \
    "cryptographic token every\n" \
    "transaction. CANNOT be\n" \
    "cloned or replayed.\n\n" \
    "LEGAL: Read your OWN NFC\n" \
    "business card or transit\n" \
    "card for learning. Study\n" \
    "ISO/IEC 14443 and NFC specs."

#define ABOUT_DOORS \
    "Criminal Trespass and\n" \
    "Burglary carry 5-20+ yrs.\n\n" \
    "Modern RFID access systems\n" \
    "(HID, MIFARE Plus, DESFire)\n" \
    "use AES-128 encryption +\n" \
    "mutual authentication.\n\n" \
    "Flipper CAN read legacy 125kHz\n" \
    "cards (EM4100, HID Prox).\n" \
    "It CANNOT crack AES-128 or\n" \
    "forge an authenticated session\n" \
    "on any modern access system.\n\n" \
    "LEGAL: Lock sport on locks\n" \
    "YOU OWN. Only pen-test with\n" \
    "written authorization."

#define ABOUT_TV \
    "Controlling your OWN TV\n" \
    "with IR is 100% legal.\n\n" \
    "WHERE IS THE IR LED?\n" \
    "It\'s the small clear dome\n" \
    "on the TOP edge of your\n" \
    "Flipper, NOT the RGB LED\n" \
    "on the front.\n\n" \
    "WHY CAN\'T I SEE IT?\n" \
    "IR light is ~940nm, outside\n" \
    "the visible spectrum (380-\n" \
    "700nm). Human eyes can\'t\n" \
    "detect it at all.\n\n" \
    "TIP: Point a phone camera\n" \
    "at the top IR LED and hold\n" \
    "OK on the demo screen.\n" \
    "Most phone cameras can see\n" \
    "IR - you\'ll see it pulse\n" \
    "purple/white on screen.\n\n" \
    "LEGAL USE: Build IR remote\n" \
    "databases, learn NEC/RC5,\n" \
    "automate YOUR home setup."

#define ABOUT_AIRPLANE \
    "18 U.S.C. S32 - Interfering\n" \
    "with aircraft systems.\n" \
    "Federal crime. Up to 20 yrs.\n\n" \
    "The Flipper TX power is too\n" \
    "low (~10 mW) to affect any\n" \
    "aviation equipment. It also\n" \
    "cannot transmit on aviation\n" \
    "bands (108-137 MHz VHF)\n" \
    "by design.\n\n" \
    "LEGAL: Get your ham radio\n" \
    "ticket! Study Part 107 for\n" \
    "drone certification instead."

#define ABOUT_JAMMER \
    "47 U.S.C. S333 - FCC:\n" \
    "Jamming ANY radio signal\n" \
    "is ILLEGAL. $100k/day fine.\n" \
    "Federal prison possible.\n\n" \
    "This includes: cell, GPS,\n" \
    "WiFi, BLE, and especially\n" \
    "911 emergency services.\n\n" \
    "The Flipper CANNOT jam cell\n" \
    "signals - wrong hardware,\n" \
    "wrong power level.\n\n" \
    "LEGAL: Get your ham license.\n" \
    "Build an SDR receiver."

#define ABOUT_ATM \
    "18 U.S.C. S1029 + S1030:\n" \
    "Bank fraud + unauthorized\n" \
    "computer access combined.\n" \
    "15-20 yrs federal prison.\n\n" \
    "Modern ATMs use EMV + TLS\n" \
    "encryption. NFC skimming\n" \
    "does NOT work on chip cards.\n" \
    "Skimmers need physical\n" \
    "installation = more crimes.\n\n" \
    "Flipper CAN read the NFC\n" \
    "layer of a card. It CANNOT\n" \
    "extract the EMV private key\n" \
    "or forge a transaction token.\n" \
    "The data it reads is useless\n" \
    "without the session crypto.\n\n" \
    "LEGAL: Study for CISA, CISSP,\n" \
    "or bank security certs."

#define ABOUT_RFJAM \
    "47 U.S.C. S333 + Part 15:\n" \
    "Sub-GHz jamming = FCC felony.\n" \
    "$100,000 fine PER DAY.\n" \
    "Equipment seizure + prison.\n\n" \
    "Sub-GHz jamming floods a\n" \
    "frequency band with noise,\n" \
    "blocking garage doors,\n" \
    "key fobs, alarm sensors,\n" \
    "AND emergency pagers.\n\n" \
    "Flipper Sub-GHz TX power is\n" \
    "~10 mW max. It can transmit\n" \
    "on allowed bands only, and\n" \
    "is NOT a true jammer.\n\n" \
    "LEGAL: Get your ham license.\n" \
    "Use SDR++ to RECEIVE only.\n" \
    "Study the Sub-GHz protocols\n" \
    "your garage door actually uses."

#define ABOUT_BLESPAM \
    "18 U.S.C. S1030 - CFAA:\n" \
    "Sending unwanted data to\n" \
    "devices without consent\n" \
    "= unauthorized access.\n\n" \
    "Unlike most items here, the\n" \
    "Flipper CAN do BLE spam.\n" \
    "It has real BLE hardware.\n" \
    "That's exactly why you need\n" \
    "to understand why NOT to.\n\n" \
    "BLE spam (fake Apple/Android\n" \
    "pairing popups, Samsung spam)\n" \
    "is illegal under CFAA and\n" \
    "computer misuse laws.\n\n" \
    "It can crash older devices,\n" \
    "drain batteries, and interfere\n" \
    "with medical BLE devices like\n" \
    "insulin pumps and hearing aids.\n\n" \
    "THAT IS NOT A JOKE.\n\n" \
    "LEGAL: Build real BLE tools.\n" \
    "Study Bluetooth Core Spec.\n" \
    "Write your own GATT profiles\n" \
    "for YOUR OWN projects."

#define CFAA_BODY \
    "Computer Fraud & Abuse Act\n" \
    "18 U.S.C. S1030\n\n" \
    "Prohibits accessing any\n" \
    "computer, device, or network\n" \
    "WITHOUT authorization.\n\n" \
    "PENALTIES:\n" \
    "  First offense: up to 5 yrs\n" \
    "  With damage: up to 10 yrs\n" \
    "  With fraud:  up to 20 yrs\n" \
    "  + civil suits on top\n\n" \
    "'I was just testing' is NOT\n" \
    "a legal defense.\n\n" \
    "USE YOUR FLIPPER LEGALLY.\n" \
    "It's an amazing learning tool."

#define APP_ABOUT_BODY \
    "SKIDcity v1.0\n" \
    "\"Don't be a SKID!\"\n\n" \
    "A Script Kiddie uses tools\n" \
    "without understanding them\n" \
    "or the law. Don't be that.\n\n" \
    "Your Flipper Zero is a\n" \
    "POWERFUL learning tool for:\n" \
    "  - Sub-GHz RF protocols\n" \
    "  - NFC / RFID research\n" \
    "  - Infrared databases\n" \
    "  - iButton / 1-Wire\n" \
    "  - GPIO & hardware hacking\n" \
    "  - BadUSB scripting\n\n" \
    "Every 'hack' in this app\n" \
    "has a LEGAL path. Choose it.\n\n" \
    "Stay curious. Stay legal."

/* ═══════════════════════════════════════════════════════
 * FEATURE DATA TABLE
 * ═══════════════════════════════════════════════════════ */
const SkidCityFeatureInfo skidcity_features[SkidCityFeatureCount] = {
    [SkidCityFeatureTraffic] = {
        .menu_label     = "Hack Traffic Lights",
        .submenu_header = "Traffic Lights",
        .demo_item      = "Traffic Light Demo",
        .about_title    = "Traffic Light Facts",
        .about_body     = ABOUT_TRAFFIC,
        .demo_type      = SkidCityDemoTrafficLed,
    },
    [SkidCityFeatureWifi] = {
        .menu_label     = "Crash WiFi Networks",
        .submenu_header = "WiFi Attacks",
        .demo_item      = "Launch Deauth Attack",
        .about_title    = "WiFi Attacks & CFAA",
        .about_body     = ABOUT_WIFI,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureCar] = {
        .menu_label     = "Steal Car Keys",
        .submenu_header = "Car Key Attacks",
        .demo_item      = "Replay Attack",
        .about_title    = "Car Key Attack Laws",
        .about_body     = ABOUT_CAR,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureCards] = {
        .menu_label     = "Clone Credit Cards",
        .submenu_header = "Payment Card Cloning",
        .demo_item      = "Clone Any Card",
        .about_title    = "Payment Card Laws",
        .about_body     = ABOUT_CARDS,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureDoors] = {
        .menu_label     = "Bypass Smart Locks",
        .submenu_header = "Smart Lock Bypass",
        .demo_item      = "Open Any Lock",
        .about_title    = "Lock Bypass Laws",
        .about_body     = ABOUT_DOORS,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureTv] = {
        .menu_label     = "Control Any TV",
        .submenu_header = "IR TV Control",
        .demo_item      = "IR Blast Demo",
        .about_title    = "IR: The Legal One!",
        .about_body     = ABOUT_TV,
        .demo_type      = SkidCityDemoIrBlink,
    },
    [SkidCityFeatureAirplane] = {
        .menu_label     = "Crash Airplane Systems",
        .submenu_header = "Aviation Systems",
        .demo_item      = "Interfere w/ Aircraft",
        .about_title    = "Aviation Interference",
        .about_body     = ABOUT_AIRPLANE,
        .demo_type      = SkidCityDemoFcc,
    },
    [SkidCityFeatureJammer] = {
        .menu_label     = "Jam Cell/GPS Signals",
        .submenu_header = "Signal Jamming",
        .demo_item      = "Jam 4G/LTE/GPS",
        .about_title    = "Signal Jamming Laws",
        .about_body     = ABOUT_JAMMER,
        .demo_type      = SkidCityDemoFcc,
    },
    [SkidCityFeatureAtm] = {
        .menu_label     = "Hack ATM / Bank",
        .submenu_header = "ATM / Bank Fraud",
        .demo_item      = "Skim ATM Cards",
        .about_title    = "Bank Fraud Laws",
        .about_body     = ABOUT_ATM,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureRfJam] = {
        .menu_label     = "Jam Sub-GHz / RF",
        .submenu_header = "Sub-GHz RF Jamming",
        .demo_item      = "Jam Garage/Alarms",
        .about_title    = "Sub-GHz Jamming Laws",
        .about_body     = ABOUT_RFJAM,
        .demo_type      = SkidCityDemoFcc,
    },
    [SkidCityFeatureBleSpam] = {
        .menu_label     = "Spam BLE / Bluetooth",
        .submenu_header = "Bluetooth LE Spam",
        .demo_item      = "Spam BLE Popups",
        .about_title    = "BLE Spam: Why Not",
        .about_body     = ABOUT_BLESPAM,
        .demo_type      = SkidCityDemoBanned,
    },
};

/* ═══════════════════════════════════════════════════════
 * HEADER SCROLL TIMER
 * ═══════════════════════════════════════════════════════ */
static void skidcity_header_timer_cb(void* context) {
    SkidCityApp* app = context;
    const char* src = HEADER_STR;
    size_t len = strlen(src);
    for(uint8_t i = 0; i < HEADER_WIN; i++) {
        app->header_buf[i] = src[(app->header_offset + i) % len];
    }
    app->header_buf[HEADER_WIN] = '\0';
    app->header_offset = (app->header_offset + 1) % len;
    submenu_set_header(app->submenu, app->header_buf);
}

/* ═══════════════════════════════════════════════════════
 * SHARED HELPERS
 * ═══════════════════════════════════════════════════════ */
static void skidcity_submenu_cb(void* context, uint32_t index) {
    SkidCityApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool skidcity_custom_event_cb(void* context, uint32_t event) {
    SkidCityApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool skidcity_back_event_cb(void* context) {
    SkidCityApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void skidcity_set_led(SkidCityApp* app, uint8_t state) {
    UNUSED(app);
    switch(state) {
    case 0: /* red */
        furi_hal_light_set(LightRed,   0xFF);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    case 1: /* yellow */
        furi_hal_light_set(LightRed,   0xFF);
        furi_hal_light_set(LightGreen, 0xFF);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    case 2: /* green */
        furi_hal_light_set(LightRed,   0x00);
        furi_hal_light_set(LightGreen, 0xFF);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    default: /* off */
        furi_hal_light_set(LightRed,   0x00);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    }
}

/* ═══════════════════════════════════════════════════════
 * TRAFFIC-LIGHT CUSTOM VIEW
 * ═══════════════════════════════════════════════════════ */
static void skidcity_traffic_draw_cb(Canvas* canvas, void* model) {
    TrafficModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Info panel (right side) */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 38, 12, "Traffic Demo");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 38, 22, "Flipper LED = your");
    canvas_draw_str(canvas, 38, 31, "\"traffic light\".");
    canvas_draw_str(canvas, 38, 42, "UP/DOWN: cycle");
    canvas_draw_str(canvas, 38, 51, "BACK: exit");

    /* Traffic light housing */
    canvas_draw_rframe(canvas, 5, 3, 26, 57, 3);
    canvas_draw_box(canvas, 15, 0, 6, 4);
    canvas_draw_box(canvas, 15, 58, 6, 5);
    canvas_draw_line(canvas, 5, 23, 30, 23);
    canvas_draw_line(canvas, 5, 40, 30, 40);

    /* Red bulb */
    if(m->state == 0) canvas_draw_disc(canvas, 18, 13, 7);
    else              canvas_draw_circle(canvas, 18, 13, 7);

    /* Yellow bulb */
    if(m->state == 1) canvas_draw_disc(canvas, 18, 31, 7);
    else              canvas_draw_circle(canvas, 18, 31, 7);

    /* Green bulb */
    if(m->state == 2) canvas_draw_disc(canvas, 18, 49, 7);
    else              canvas_draw_circle(canvas, 18, 49, 7);

    /* State label */
    const char* labels[] = {"RED", "YELLOW", "GREEN"};
    canvas_set_font(canvas, FontPrimary);
    if(m->state < 3) {
        canvas_draw_str_aligned(canvas, 82, 58, AlignCenter, AlignBottom, labels[m->state]);
    }
}

static bool skidcity_traffic_input_cb(InputEvent* event, void* context) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    SkidCityApp* app = context;

    if(event->key == InputKeyBack) return false;

    if(event->key == InputKeyUp || event->key == InputKeyRight) {
        uint8_t ns;
        with_view_model(
            app->traffic_view, TrafficModel* model,
            { model->state = (model->state < 2) ? model->state + 1 : 0; ns = model->state; },
            true);
        skidcity_set_led(app, ns);
        return true;
    }
    if(event->key == InputKeyDown || event->key == InputKeyLeft) {
        uint8_t ns;
        with_view_model(
            app->traffic_view, TrafficModel* model,
            { model->state = (model->state > 0) ? model->state - 1 : 2; ns = model->state; },
            true);
        skidcity_set_led(app, ns);
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════
 * IR DEMO CUSTOM VIEW
 * Shows instructions + "TRANSMITTING" status.
 * OK hold: fires real IR + flashes red LED.
 * ═══════════════════════════════════════════════════════ */
static void skidcity_ir_draw_cb(Canvas* canvas, void* model) {
    IrDemoModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "IR Transmitter Demo");

    canvas_draw_line(canvas, 0, 12, 128, 12);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, "Blue LED just blinked?");
    canvas_draw_str(canvas, 2, 33, "Just kidding. That's the");
    canvas_draw_str(canvas, 2, 43, "RGB LED, not the IR TX.");

    if(m->transmitting) {
        /* Bold transmitting indicator */
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 14, 50, 100, 13, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "*** TRANSMITTING ***");
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "Hold OK to transmit IR");
    }
}

static bool skidcity_ir_input_cb(InputEvent* event, void* context) {
    SkidCityApp* app = context;

    if(event->key == InputKeyOk && event->type == InputTypePress) {
        /* Start IR transmission */
        skidcity_ir_tx_state.pos  = 0;
        skidcity_ir_tx_state.done = false;
        furi_hal_infrared_async_tx_set_data_isr_callback(skidcity_ir_isr_cb, NULL);
        furi_hal_infrared_async_tx_start(38000, 0.33f);
        /* Red LED as visible "transmitting" indicator */
        furi_hal_light_set(LightRed,  0xFF);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue,  0x00);
        with_view_model(
            app->ir_view, IrDemoModel* model,
            { model->transmitting = true; },
            true);
        return true;
    }

    if(event->key == InputKeyOk &&
       (event->type == InputTypeRelease || event->type == InputTypeShort)) {
        /* Stop IR and LED */
        furi_hal_infrared_async_tx_stop();
        furi_hal_light_set(LightRed,   0x00);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue,  0x00);
        with_view_model(
            app->ir_view, IrDemoModel* model,
            { model->transmitting = false; },
            true);
        return true;
    }

    if(event->key == InputKeyBack) return false;
    return false;
}

/* ═══════════════════════════════════════════════════════
 * BANNED CUSTOM VIEW
 * ═══════════════════════════════════════════════════════ */
static void skidcity_banned_draw_cb(Canvas* canvas, void* model) {
    BannedModel* m = model;

    /* Black background */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 64);

    /* White border */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rframe(canvas, 1, 1, 126, 62, 5);

    canvas_set_font(canvas, FontPrimary);

    if(m->variant == SkidCityBannedTypeFCC) {
        canvas_draw_str_aligned(canvas, 64, 7,  AlignCenter, AlignTop, "FCC VIOLATION");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "47 U.S.C. S333");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Signal jamming is ILLEGAL.");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "$100,000 fine + prison.");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[BACK = learn why]");
    } else if(m->variant == SkidCityBannedTypeSkid) {
        canvas_draw_str_aligned(canvas, 64, 7,  AlignCenter, AlignTop, "DON'T BE A");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "SCRIPT KIDDIE!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Use tools you UNDERSTAND.");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "Learn the fundamentals.");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[BACK = learn why]");
    } else {
        /* CFAA default */
        canvas_draw_str_aligned(canvas, 64, 7,  AlignCenter, AlignTop, "YOUR FLIPPER");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "IS BANNED");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "18 U.S.C. S1030 - CFAA");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "Unauthorized Access");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[BACK = learn why]");
    }
}

static bool skidcity_banned_input_cb(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    /* Let BACK bubble up to the scene manager navigation handler */
    return false;
}

/* ═══════════════════════════════════════════════════════
 * SCENE: MAIN MENU
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_main_menu_on_enter(void* context) {
    SkidCityApp* app = context;
    submenu_reset(app->submenu);

    /* Seed the first frame of the scrolling header, then start the timer */
    app->header_offset = 0;
    skidcity_header_timer_cb(app);
    app->header_timer = furi_timer_alloc(skidcity_header_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->header_timer, 150);

    for(uint32_t i = 0; i < SkidCityFeatureCount; i++) {
        submenu_add_item(
            app->submenu, skidcity_features[i].menu_label,
            i, skidcity_submenu_cb, app);
    }
    submenu_add_item(
        app->submenu, "-- About SKIDcity --",
        SkidCityFeatureCount, skidcity_submenu_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewSubmenu);
}

bool skidcity_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    SkidCityApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == (uint32_t)SkidCityFeatureCount) {
        scene_manager_next_scene(app->scene_manager, SkidCitySceneAppAbout);
        return true;
    }
    if(event.event < (uint32_t)SkidCityFeatureCount) {
        app->current_feature = (SkidCityFeature)event.event;
        scene_manager_next_scene(app->scene_manager, SkidCitySceneFeatureMenu);
        return true;
    }
    return false;
}

void skidcity_scene_main_menu_on_exit(void* context) {
    SkidCityApp* app = context;
    furi_timer_stop(app->header_timer);
    furi_timer_free(app->header_timer);
    app->header_timer = NULL;
    submenu_reset(app->submenu);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: FEATURE SUB-MENU
 * ═══════════════════════════════════════════════════════ */
#define FEAT_EV_DEMO  0
#define FEAT_EV_ABOUT 1
#define FEAT_EV_CFAA  2

void skidcity_scene_feature_menu_on_enter(void* context) {
    SkidCityApp* app = context;
    const SkidCityFeatureInfo* fi = &skidcity_features[app->current_feature];

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, fi->submenu_header);

    submenu_add_item(
        app->submenu, fi->demo_item,
        FEAT_EV_DEMO, skidcity_submenu_cb, app);
    submenu_add_item(
        app->submenu, "Why is this Illegal?",
        FEAT_EV_ABOUT, skidcity_submenu_cb, app);

    if(fi->demo_type == SkidCityDemoBanned ||
       fi->demo_type == SkidCityDemoFcc) {
        submenu_add_item(
            app->submenu, "CFAA / Full Legal Info",
            FEAT_EV_CFAA, skidcity_submenu_cb, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewSubmenu);
}

bool skidcity_scene_feature_menu_on_event(void* context, SceneManagerEvent event) {
    SkidCityApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    const SkidCityFeatureInfo* fi = &skidcity_features[app->current_feature];

    if(event.event == FEAT_EV_DEMO) {
        switch(fi->demo_type) {
        case SkidCityDemoTrafficLed:
            with_view_model(
                app->traffic_view, TrafficModel* model,
                { model->state = 0; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneTrafficDemo);
            return true;
        case SkidCityDemoIrBlink:
            scene_manager_next_scene(app->scene_manager, SkidCitySceneGenericDemo);
            return true;
        case SkidCityDemoBanned:
            with_view_model(
                app->banned_view, BannedModel* model,
                { model->variant = SkidCityBannedTypeCFAA; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneBanned);
            return true;
        case SkidCityDemoSkid:
            with_view_model(
                app->banned_view, BannedModel* model,
                { model->variant = SkidCityBannedTypeSkid; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneBanned);
            return true;
        case SkidCityDemoFcc:
            with_view_model(
                app->banned_view, BannedModel* model,
                { model->variant = SkidCityBannedTypeFCC; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneBanned);
            return true;
        }
    }
    if(event.event == FEAT_EV_ABOUT) {
        scene_manager_next_scene(app->scene_manager, SkidCitySceneFeatureAbout);
        return true;
    }
    if(event.event == FEAT_EV_CFAA) {
        scene_manager_next_scene(app->scene_manager, SkidCitySceneCfaaInfo);
        return true;
    }
    return false;
}

void skidcity_scene_feature_menu_on_exit(void* context) {
    SkidCityApp* app = context;
    submenu_reset(app->submenu);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: TRAFFIC DEMO
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_traffic_demo_on_enter(void* context) {
    SkidCityApp* app = context;
    skidcity_set_led(app, 0); /* start on red */
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewTrafficDemo);
}

bool skidcity_scene_traffic_demo_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_traffic_demo_on_exit(void* context) {
    SkidCityApp* app = context;
    UNUSED(app);
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue,  0x00);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: GENERIC DEMO (IR blink)
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_generic_demo_on_enter(void* context) {
    SkidCityApp* app = context;
    /* Reset transmitting state on entry */
    with_view_model(
        app->ir_view, IrDemoModel* model,
        { model->transmitting = false; },
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewIrDemo);
}

bool skidcity_scene_generic_demo_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_generic_demo_on_exit(void* context) {
    SkidCityApp* app = context;
    /* Always ensure IR and LED are off when leaving */
    furi_hal_infrared_async_tx_stop();
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue,  0x00);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: FEATURE ABOUT ("Why is this Illegal?")
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_feature_about_on_enter(void* context) {
    SkidCityApp* app = context;
    const SkidCityFeatureInfo* fi = &skidcity_features[app->current_feature];

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, fi->about_title);
    widget_add_text_scroll_element(
        app->widget, 0, 18, 128, 46, fi->about_body);

    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewWidget);
}

bool skidcity_scene_feature_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_feature_about_on_exit(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: BANNED
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_banned_on_enter(void* context) {
    SkidCityApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewBanned);
}

bool skidcity_scene_banned_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_banned_on_exit(void* context) {
    UNUSED(context);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: CFAA INFO (deep-dive legal text)
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_cfaa_info_on_enter(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "The Law: CFAA");
    widget_add_text_scroll_element(
        app->widget, 0, 18, 128, 46, CFAA_BODY);
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewWidget);
}

bool skidcity_scene_cfaa_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_cfaa_info_on_exit(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: APP ABOUT
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_app_about_on_enter(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "About SKIDcity");
    widget_add_text_scroll_element(
        app->widget, 0, 18, 128, 46, APP_ABOUT_BODY);
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewWidget);
}

bool skidcity_scene_app_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_app_about_on_exit(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
}

/* ═══════════════════════════════════════════════════════
 * APP LIFECYCLE
 * ═══════════════════════════════════════════════════════ */
static SkidCityApp* skidcity_app_alloc(void) {
    SkidCityApp* app = malloc(sizeof(SkidCityApp));
    furi_check(app);

    app->current_feature = SkidCityFeatureTraffic;
    app->header_timer   = NULL;
    app->header_offset  = 0;

    /* Core services */
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* SceneManager */
    app->scene_manager = scene_manager_alloc(&skidcity_scene_handlers, app);

    /* ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, skidcity_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, skidcity_back_event_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Submenu view */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewSubmenu, submenu_get_view(app->submenu));

    /* Widget view */
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewWidget, widget_get_view(app->widget));

    /* Traffic-light custom view */
    app->traffic_view = view_alloc();
    view_set_context(app->traffic_view, app);
    view_allocate_model(app->traffic_view, ViewModelTypeLocking, sizeof(TrafficModel));
    view_set_draw_callback(app->traffic_view, skidcity_traffic_draw_cb);
    view_set_input_callback(app->traffic_view, skidcity_traffic_input_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewTrafficDemo, app->traffic_view);

    /* Banned custom view */
    app->banned_view = view_alloc();
    view_set_context(app->banned_view, app);
    view_allocate_model(app->banned_view, ViewModelTypeLocking, sizeof(BannedModel));
    view_set_draw_callback(app->banned_view, skidcity_banned_draw_cb);
    view_set_input_callback(app->banned_view, skidcity_banned_input_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewBanned, app->banned_view);

    /* IR demo custom view */
    app->ir_view = view_alloc();
    view_set_context(app->ir_view, app);
    view_allocate_model(app->ir_view, ViewModelTypeLocking, sizeof(IrDemoModel));
    view_set_draw_callback(app->ir_view, skidcity_ir_draw_cb);
    view_set_input_callback(app->ir_view, skidcity_ir_input_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewIrDemo, app->ir_view);

    return app;
}

static void skidcity_app_free(SkidCityApp* app) {
    /* Ensure LED is off on exit */
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue,  0x00);

    /* Remove views before freeing */
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewTrafficDemo);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewBanned);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewIrDemo);

    submenu_free(app->submenu);
    widget_free(app->widget);
    view_free(app->traffic_view);
    view_free(app->banned_view);
    view_free(app->ir_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t skidcity_app(void* p) {
    UNUSED(p);
    SkidCityApp* app = skidcity_app_alloc();
    scene_manager_next_scene(app->scene_manager, SkidCitySceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);
    skidcity_app_free(app);
    return 0;
}
