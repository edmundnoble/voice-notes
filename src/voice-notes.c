#include <pebble.h>

static Window *window;
static TextLayer *text_layer;

static bool taking_note = false;

#define PERSIST_KEY_NUM_NOTES 0
#define PERSIST_KEY_FIRST_NOTE 1
#define MAX_NOTE_SIZE 500

static uint32_t current_note_idx = 0;
static char current_note[MAX_NOTE_SIZE];
static DictationSession *dict_session;

static void dictation_status_handler(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if (status == DictationSessionStatusSuccess) {
    uint32_t num_notes = (uint32_t)persist_read_int(PERSIST_KEY_NUM_NOTES);
    persist_write_string(num_notes + PERSIST_KEY_FIRST_NOTE, transcription);
    persist_write_int(PERSIST_KEY_NUM_NOTES, (int32_t)(num_notes + 1));
  }
}

static void start_taking_note(void) {
  if (!dict_session) {
    dict_session = dictation_session_create(MAX_NOTE_SIZE, dictation_status_handler, NULL);
    dictation_session_enable_confirmation(dict_session, true);
  }
  dictation_session_start(dict_session);
}

static void stop_taking_note(void) {
  if (dict_session) {
    dictation_session_stop(dict_session);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!taking_note) {
    start_taking_note();
  } else {
    stop_taking_note();
  }
}

static void reload_note() {
  persist_read_string(PERSIST_KEY_FIRST_NOTE + current_note_idx, current_note, MAX_NOTE_SIZE);
  text_layer_set_text(text_layer, current_note);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  uint32_t num_notes = (uint32_t)persist_read_int(PERSIST_KEY_NUM_NOTES);
  if ((current_note_idx + 1) < num_notes) {
    current_note_idx += 1;
    reload_note();
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if ((current_note_idx - 1) > 0) {
    current_note_idx -= 1;
    reload_note();
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(text_layer, "Press a button");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
  if (!persist_exists(PERSIST_KEY_NUM_NOTES)) {
    persist_write_int(PERSIST_KEY_NUM_NOTES, 0);
  }
  if (!persist_exists(PERSIST_KEY_FIRST_NOTE)) {
    persist_write_string(PERSIST_KEY_FIRST_NOTE, "Example note");
  }
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
