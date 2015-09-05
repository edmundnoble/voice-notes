#include <pebble.h>

#define PERSIST_KEY_NUM_NOTES 0
#define PERSIST_KEY_FIRST_NOTE 1
#define MAX_NOTE_SIZE PERSIST_STRING_MAX_LENGTH
#define NUM_ACTIONS 2

Window *window;

uint32_t current_note_idx = 0; // TODO

char **notes;
TextLayer **note_layers;
uint32_t num_notes = 0;

DictationSession *dict_session;
ActionMenu *action_menu;
ActionMenuLevel *root_level;
ScrollLayer *note_scroll_layer;
ScrollLayerCallbacks scroll_layer_callbacks;
GFont note_font;

typedef enum {
    ActionTypeRecord,
    ActionTypeDelete,
} ActionType;

typedef struct {
    ActionType type;
} Context;

static void redraw_notes(void) {
  uint32_t saved_note_idx = current_note_idx;
  Layer *window_layer = window_get_root_layer(window);
  layer_remove_child_layers(window_layer);
  if (note_scroll_layer) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Old scroll layer exists! Deleting");
    layer_remove_child_layers(scroll_layer_get_layer(note_scroll_layer));
    scroll_layer_destroy(note_scroll_layer);
  }
  GRect bounds = layer_get_bounds(window_layer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Making new scroll layer...");
  GSize scroll_content_bounds = GSize(bounds.size.w, num_notes * bounds.size.h);
  note_scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_callbacks(note_scroll_layer, scroll_layer_callbacks);
  scroll_layer_set_click_config_onto_window(note_scroll_layer, window);
  if (note_layers) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Old note layers exist! Deleting");
    for (uint32_t i = 0; i < num_notes; i++) {
      free(note_layers[i]);
    }
    free(note_layers);
  }
  note_layers = malloc(sizeof(TextLayer*) * num_notes);
  for (uint32_t i = 0; i < num_notes; i++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Creating note layer for note with content %s", notes[i]);
    note_layers[i] = text_layer_create(GRect(0, i * bounds.size.h, bounds.size.w, bounds.size.h));
    text_layer_set_text(note_layers[i], notes[i]);

    GFont note_font;

    switch (strlen(notes[i]) / 20) {
      case 0:
      case 1:
        note_font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
        break;
      case 2: 
        note_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
        break;
      case 3:
        note_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
        break;
      default:
        note_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
        break;
    }
    text_layer_set_font(note_layers[i], note_font);
    scroll_layer_add_child(note_scroll_layer, text_layer_get_layer(note_layers[i]));
    //text_layer_enable_paging(note_layers[i], bounds.size.h, 0);
  }
  scroll_layer_set_content_size(note_scroll_layer, scroll_content_bounds);
  scroll_layer_enable_paging(note_scroll_layer, 168);
  layer_add_child(window_layer, scroll_layer_get_layer(note_scroll_layer));
  current_note_idx = saved_note_idx;
  scroll_layer_set_content_offset(note_scroll_layer, GPoint(0, current_note_idx * -168), false);
  current_note_idx = saved_note_idx;
}

static void load_all_notes(void) {
  if (notes) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Old notes exist! Deleting");
    for (uint32_t i = 0; i < num_notes; i++) {
      if (notes[i]) free(notes[i]);
    }
    free(notes);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Creating new notes with size %d", (int)(sizeof(char*) * num_notes));
  notes = malloc(sizeof(char*) * num_notes);
  for (uint32_t i = 0; i < num_notes; i++) {
    int note_size = persist_get_size(PERSIST_KEY_FIRST_NOTE + i);
    char *note = malloc(sizeof(char) * note_size);
    persist_read_string(PERSIST_KEY_FIRST_NOTE + i, note, note_size);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded note %d with content: %s", (int)i, note);
    notes[i] = note;
  }
  redraw_notes();
}

static void log_status(DictationSessionStatus status) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Dictation session status: %d", (int)status);
}

static void dictation_status_handler(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  log_status(status);
  if (status == DictationSessionStatusSuccess) {
    persist_write_string(num_notes + PERSIST_KEY_FIRST_NOTE, transcription);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "New note written: %s", transcription);
    num_notes += 1;
    persist_write_int(PERSIST_KEY_NUM_NOTES, (int32_t)num_notes);
  }
}

static void start_taking_note(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Taking a note");
  if (!dict_session) {
    dict_session = dictation_session_create(MAX_NOTE_SIZE, dictation_status_handler, NULL);
    dictation_session_enable_confirmation(dict_session, true);
  }
  dictation_session_start(dict_session);
}

static void timer_callback_stub(void *callback_data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "In timer callback!");
  start_taking_note();
}

static void delete_current_note(void) {
  if (current_note_idx == 0) return; // TODO
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Tried to delete note: %d", (int)current_note_idx);
  char note_buf[MAX_NOTE_SIZE];
  for (uint32_t i = current_note_idx + 1; i < num_notes; i++) {
      persist_read_string(PERSIST_KEY_FIRST_NOTE + i, note_buf, MAX_NOTE_SIZE);
      persist_write_string(PERSIST_KEY_FIRST_NOTE + i - 1, note_buf);
  }
  persist_delete(PERSIST_KEY_FIRST_NOTE + num_notes);
  num_notes -= 1;
  persist_write_int(PERSIST_KEY_NUM_NOTES, num_notes);
  if (current_note_idx + 1 == num_notes) {
    current_note_idx -= 1;
  }
  load_all_notes();
}

static void action_performed_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  Context ctx = *(Context*)action_menu_item_get_action_data(action);
  ActionType type = *(ActionType*)action_menu_item_get_action_data(action);//ctx.type;
  switch (type) {
    case ActionTypeRecord:
      start_taking_note();
      break;
    case ActionTypeDelete:
      delete_current_note();
      break;
    }
    action_menu_close(action_menu, true);
    action_menu = NULL;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  static ActionMenuConfig config;
  config = (ActionMenuConfig) {
    .root_level = root_level,
    .colors = {
      .background = GColorChromeYellow,
      .foreground = GColorBlack,
    },
    .align = ActionMenuAlignCenter
  };
  action_menu = action_menu_open(&config);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void content_offset_changed_handler(ScrollLayer *scroll_layer, void *context) {
  current_note_idx = (scroll_layer_get_content_offset(scroll_layer).y / -168);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Current note: %d", (int)current_note_idx);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  note_font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  
  root_level = action_menu_level_create(NUM_ACTIONS);
  static Context record_ctx, delete_ctx;
  record_ctx = (Context) { .type = ActionTypeRecord }, delete_ctx = (Context) { .type = ActionTypeDelete };
  
  action_menu_level_add_action(root_level, "Record Note", action_performed_callback, &record_ctx);
  action_menu_level_add_action(root_level, "Delete Note", action_performed_callback, &delete_ctx);

  scroll_layer_callbacks = (struct ScrollLayerCallbacks) {
    .click_config_provider = click_config_provider,
    .content_offset_changed_handler = content_offset_changed_handler
  };

  load_all_notes();

  if (launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Registering timer callback!");
    app_timer_register(0, timer_callback_stub, NULL);
    //start_taking_note();
  }
}

static void window_unload(Window *window) {
  //layer_destroy(current_note_layer);
  if (note_scroll_layer) scroll_layer_destroy(note_scroll_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  if (!persist_exists(PERSIST_KEY_NUM_NOTES)) {
    persist_write_int(PERSIST_KEY_NUM_NOTES, 1);
    num_notes = 1;
  } else {
    num_notes = (uint32_t)persist_read_int(PERSIST_KEY_NUM_NOTES);
  }
  if (!persist_exists(PERSIST_KEY_FIRST_NOTE)) {
    persist_write_string(PERSIST_KEY_FIRST_NOTE, "Example note");
  }
  window_stack_push(window, true /* animated */);
}

static void deinit(void) {
  window_destroy(window);
  if (dict_session) {
    dictation_session_destroy(dict_session);
  }
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
