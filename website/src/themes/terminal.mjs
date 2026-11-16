// Terminal Markdown's palette applied to fenced code through Expressive Code.
export default {
  name: 'terminal',
  type: 'dark',
  colors: {
    'editor.background': '#101a13',
    'editor.foreground': '#acd8b7',
    'editor.selectionBackground': '#2f7250',
    'editorLineNumber.foreground': '#8fa997',
    'editorWidget.background': '#0b0f0c',
    'editorWidget.border': '#254630',
    'editorGroupHeader.tabsBorder': '#254630',
    focusBorder: '#33d17a',
  },
  tokenColors: [
    { scope: 'comment', settings: { foreground: '#8fa997', fontStyle: 'italic' } },
    { scope: ['string', 'markup.inserted'], settings: { foreground: '#86e7a4' } },
    { scope: ['constant.numeric', 'constant.language'], settings: { foreground: '#f0c674' } },
    { scope: ['keyword', 'storage'], settings: { foreground: '#c8b1ff' } },
    { scope: 'keyword.operator', settings: { foreground: '#6bdac3' } },
    {
      scope: ['entity.name.function', 'entity.name.type', 'support.function'],
      settings: { foreground: '#7bc8ff' },
    },
  ],
};
