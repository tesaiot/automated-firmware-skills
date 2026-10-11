# UI spec format `pse84-ui/1`

One JSON file (`<app>/ui/ui.json`) describes the whole UI; `ui_gen.py` turns it into static LVGL 9.5
C code. Checked by `ui_spec.py check` (the editor and `ui_ctl.py` refuse invalid specs).
Example: `../examples/lvgl-demo.json`.

```json
{
  "spec": "pse84-ui/1",
  "name": "my-ui",
  "display": {"width": 800, "height": 480},
  "theme": {"dark": true, "primary": "#2196F3", "secondary": "#F44336", "font": "montserrat_14"},
  "data": [{"id": "level", "init": 0, "min": 0, "max": 100, "source": "free text: where it comes from"}],
  "screens": [{"id": "main", "style": {"bg": "#101820"}, "layout": null, "children": [ ...widgets ]}]
}
```

## Ids
`lower_snake_case`, max 32 characters, unique across screens, widgets and data together
(they become `ui.<id>`, `ui_data_<id>`, `UI_W_<ID>`, `ui_on_<id>_<event>()`). Keep ids stable:
renaming one orphans its handler in `ui_actions.c`.

## Widgets
Common keys: `id`, `type`, `align`, `x`, `y`, `w`, `h`, `hidden`, `style`, `grow`, `on`, `note`.
`children` only on `container` and `button` (the validator rejects it elsewhere). To put a label
on an arc or slider, wrap both in a transparent container (`"style": {"bg_opa": 0,
"border_width": 0, "pad": 0}`) and align each child, e.g. both `center`.

| type | own keys | LVGL | bind (two-way) |
|---|---|---|---|
| `container` | `layout`, `children` | `lv_obj` (card: bg, border, pad 20) | - |
| `label` | `text`, `bind`+`fmt`, `long_mode` (wrap, dot, scroll, clip) | `lv_label` | shows the value via `fmt` (exactly one `%d`, `%%` for %) |
| `button` | `text`, `layout`, `children`, `checkable` | `lv_button` + centred label | checked state (needs `checkable`) |
| `slider` / `bar` / `arc` | `min`, `max`, `value`, `bind` | `lv_slider` / `lv_bar` / `lv_arc` | value |
| `switch` | `checked`, `bind` | `lv_switch` | checked 0/1 |
| `checkbox` | `text`, `checked`, `bind` | `lv_checkbox` | checked 0/1 |
| `dropdown` | `options` [..], `selected`, `bind` | `lv_dropdown` | selected index |
| `spinner` | - | `lv_spinner` | - |

Position: `align` (top_left, top_mid, top_right, left_mid, center, right_mid, bottom_left,
bottom_mid, bottom_right; default top_left) + `x`/`y` offsets in px, relative to the parent's content
area. Ignored inside a flex parent (then the order of `children` is the order on screen; `grow`
shares free space).
Size: `w`/`h` = px, `"content"` or `"N%"`; omitted = the LVGL default (container 130x130, slider/bar
260x13, arc/spinner 130x130, switch 52x30, dropdown 130 wide; label/button/checkbox fit content).

`layout` (container, button, screen): `{"flex": "row|column|row_wrap|column_wrap", "gap": px,
"main": start|center|end|space_between|space_around|space_evenly, "cross": start|center|end}`.

`style` (all optional): `bg`, `bg_opa` (0..100 %), `text_color`, `font` (`montserrat_8` ..
`montserrat_48`, even sizes; each size used is compiled in), `radius`, `border_width`,
`border_color`, `pad`, `gap`, `text_align` (left, center, right), `shadow` (px), `accent`
(indicator/knob colour of slider, bar, arc, switch, checkbox, spinner; background of a button).
Colours are `#RRGGBB`. Text uses ASCII only (Montserrat has no other glyphs).

## Data
`data` items are LVGL int subjects (`lv_subject_t ui_data_<id>`): every widget with
`"bind": "<id>"` follows the value and, if it is an input, writes it back. `min`/`max` clamp every
write (`ui_data_set`). From code: `ui_data_set(UI_D_<ID>, v)`, `ui_data_get(UI_D_<ID>)`, or the
LVGL subject directly. From the CM33 console: `uiset <id> <v>` / `uiget <id>`. `source` is a
note for the developer (where the value should come from), it generates no code.

## Events
`"on": {"<event>": <action> | [<action>, ...]}`, events `clicked`, `pressed`, `released`,
`long_pressed`, `value_changed`. Actions:

| action | generated code |
|---|---|
| `"user"` | calls `ui_on_<id>_<event>(e)` in `ui_actions.c` (stub appended once, yours to fill) |
| `{"goto": "<screen>"}` | `lv_screen_load(ui.<screen>)` |
| `{"set": "<data>", "value": n}` | `ui_data_set(...)` |
| `{"inc": "<data>", "by": n}` | add n (default 1; negative = decrement), clamped to min..max |
| `{"toggle": "<data>"}` | 0 <-> 1 |

## Limits
800x480 only (Waveshare 4.3" DSI); max 255 widgets, 255 data items, 16 screens. All screens are
created at start (fast switching, RAM for every screen). No images yet (needs C arrays + memory
placement, and the VG-Lite tiled-image patch).

## JSON Patch with `#id` (ui_ctl.py patch)
RFC 6902 (`add`, `remove`, `replace`, `move`, `copy`, `test`). A path segment `#<id>` jumps to
that screen, widget or data item, so nested pointers are never needed:
```json
[{"op": "replace", "path": "/#title/text", "value": "Hello"},
 {"op": "add", "path": "/#left_col/children/-", "value": {"id": "ok_btn", "type": "button", "text": "OK"}},
 {"op": "add", "path": "/#main/children/0", "value": {"id": "bg_box", "type": "container"}},
 {"op": "remove", "path": "/#old_label"},
 {"op": "move", "from": "/#ok_btn", "path": "/#right_col/children/0"},
 {"op": "add", "path": "/data/-", "value": {"id": "temp", "init": 0, "min": -40, "max": 125}}]
```
