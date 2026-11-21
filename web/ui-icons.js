/* Small, local UI glyphs share the same stroke and view box. */
const riftIcons = {
  nest: '<path d="m3 11 9-8 9 8"/><path d="M5.5 10v9.5h13V10"/><path d="m9 13 3-3 3 3-3 4z"/>',
  wand: '<path d="m3 21 12-12 3 3L6 24M13 11l3 3" transform="translate(0 -2)"/><path d="m17 2 .8 2.4L20 5l-2.2.8L17 8l-.8-2.2L14 5l2.2-.6zM6 4v4M4 6h4m12 9v4m-2-2h4"/>',
  portal: '<path d="M3 21h18M5 20v-8a7 7 0 0 1 14 0v8M8.5 20v-7a3.5 3.5 0 0 1 7 0v7"/><path d="m12 2 .7 2.1L15 5l-2.3.8L12 8l-.7-2.2L9 5l2.3-.9z"/>',
  users: '<circle cx="9" cy="8" r="3"/><path d="M2.5 20v-2.2A4.8 4.8 0 0 1 7.3 13h3.4a4.8 4.8 0 0 1 4.8 4.8V20zM16 5.5a3 3 0 0 1 0 5.7m1.5 2.5a4.4 4.4 0 0 1 4 4.4V20"/>',
  rune: '<path d="m12 2 7 8-7 12-7-12z"/><path d="m12 6 3.5 4-3.5 6-3.5-6z"/>',
  device: '<rect x="6.5" y="2" width="11" height="20" rx="2.7"/><rect x="9" y="5" width="6" height="9" rx=".7"/><path d="M11.5 18h1"/>',
  help: '<circle cx="12" cy="12" r="9"/><path d="M9.4 9a2.7 2.7 0 1 1 4.3 2.2c-1.2.8-1.7 1.2-1.7 2.8m0 2.9v.1"/>',
  close: '<path d="m6 6 12 12M18 6 6 18"/>',
  menu: '<path d="M3 6h18M3 12h18M3 18h18"/>',
  chevron: '<path d="m9 5 7 7-7 7"/>',
  arrow: '<path d="M4 12h15m-6-6 6 6-6 6"/>',
  back: '<path d="M20 12H5m6 6-6-6 6-6"/>',
  compass: '<circle cx="12" cy="12" r="9"/><path d="m15.7 8.3-2.4 5-5 2.4 2.4-5z"/>',
  volume: '<path d="M3 9h4l5-4v14l-5-4H3zM16 9a4 4 0 0 1 0 6m2.5-9a8 8 0 0 1 0 12"/>',
  mute: '<path d="M3 9h4l5-4v14l-5-4H3zM16 9l5 6m0-6-5 6"/>',
  sparkle: '<path d="m12 3 2.2 6.1L20 12l-5.8 2.4L12 21l-2.3-6.6L4 12l5.7-2.9zM20 2v4m-2-2h4"/>',
  snow: '<path d="M12 2v20M3.3 7l17.4 10M3.3 17l17.4-10M9 4l3 3 3-3M9 20l3-3 3 3M4 10l4-1-1-4m10 14-1-4 4-1M4 14l4 1-1 4M17 5l-1 4 4 1"/>',
  fire: '<path d="M12 2c2 6-1 7-1 9 2 0 4-2 5-4 3 3 4 6 3 9a7.5 7.5 0 0 1-14-.5c-1-3 1-6 3-8-1 4 2 4 2 1s1-5 2-6z"/>',
  shield: '<path d="m12 2 8 4v6c0 4.5-4.2 8.4-8 10-3.8-1.6-8-5.5-8-10V6z"/><path d="m8.5 12 2.5 2.5 4.5-5"/>',
  leaf: '<path d="M3 17C2 8 10 6 20 3c1 9-2 17-11 16m-6 2L15 9"/>',
  swords: '<path d="m3 3 5 1 11 12-3 3L4 8zm13 0 5-1-1 5-5 5M9 15l-3 4m-2-4 5 5m6-4 5 5m-1-6-4 5"/>',
  book: '<path d="M12 6v15m0-15C9 3 5 3 2 4v14c4-1 7-1 10 3 3-4 6-4 10-3V4c-3-1-7-1-10 2z"/>',
  history: '<path d="M3 11a9 9 0 1 1 1.7 6M3 5v6h6m3-4v5l3 2"/>',
  trophy: '<path d="M8 4h8v5c0 4-2 6-4 6s-4-2-4-6zM8 6H4v2c0 3 2 4 5 4m7-6h4v2c0 3-2 4-5 4m-3 3v4m-4 2h8"/>',
  heart: '<path d="M20.3 4.8a5.4 5.4 0 0 0-7.6 0l-.7.8-.7-.8a5.4 5.4 0 0 0-7.6 7.7L12 21l8.3-8.5a5.4 5.4 0 0 0 0-7.7z"/>',
  bolt: '<path d="m13 2-9 12h7l-1 8 10-13h-7z"/>',
  link: '<path d="m10 14 4-4m-6 1-2 2a3.5 3.5 0 1 0 5 5l3-3m2-2 2-2a3.5 3.5 0 1 0-5-5l-3 3"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  check: '<path d="m4 12 5 5L20 6"/>',
  target: '<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="4"/><circle cx="12" cy="12" r=".6"/>',
  light: '<path d="M8 14a6 6 0 1 1 8 0c-1 .8-1.5 1.5-1.5 3h-5c0-1.5-.5-2.2-1.5-3zM10 20h4"/>',
  play: '<path d="m8 4 12 8-12 8z"/>',
  hand: '<path d="M5 13V9a1.5 1.5 0 0 1 3 0v3-7a1.5 1.5 0 0 1 3 0v7-9a1.5 1.5 0 0 1 3 0v9-7a1.5 1.5 0 0 1 3 0v8l1.6-1.7a1.8 1.8 0 0 1 2.6 2.4l-4.2 6a5 5 0 0 1-4 2.1h-2.2a5 5 0 0 1-4.6-3.1L3.3 14a1 1 0 0 1 1.7-1z"/>',
  info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v5m0-9v.2"/>'
};

function icon(name, className = '') {
  return '<svg class="icon ' + className + '" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.65" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + (riftIcons[name] || '') + '</svg>';
}
