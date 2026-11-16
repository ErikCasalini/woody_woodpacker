// MarkText's Cyberdream Prism palette mapped to Expressive Code's TextMate scopes.
export default {
  name: 'cyberdream',
  type: 'dark',
  colors: {
    'editor.background': '#1e2124',
    'editor.foreground': '#ffffff',
    'editor.selectionBackground': '#2d3035',
    'editorLineNumber.foreground': '#7b8496',
    'editorWidget.background': '#121416',
    'editorWidget.border': '#3c4048',
    focusBorder: '#5ea1ff',
  },
  tokenColors: [
    { scope: ['comment', 'meta.tag.sgml'], settings: { foreground: '#7b8496' } },
    { scope: 'punctuation', settings: { foreground: '#ffffff' } },
    {
      scope: ['constant', 'entity.name.tag', 'support.type.property-name', 'markup.deleted'],
      settings: { foreground: '#ff6e5e' },
    },
    {
      scope: ['constant.numeric', 'constant.language.boolean'],
      settings: { foreground: '#ffbd5e' },
    },
    {
      scope: ['string', 'constant.character', 'entity.other.attribute-name', 'markup.inserted'],
      settings: { foreground: '#5eff6c' },
    },
    {
      scope: ['keyword.operator', 'constant.character.entity', 'markup.underline.link'],
      settings: { foreground: '#5ef1ff' },
    },
    {
      scope: ['entity.name.function', 'support.function', 'entity.name.type', 'keyword.other.directive'],
      settings: { foreground: '#5ea1ff' },
    },
    { scope: ['keyword', 'storage'], settings: { foreground: '#bd5eff' } },
    {
      scope: ['variable', 'string.regexp', 'keyword.other.important'],
      settings: { foreground: '#ff5ef1' },
    },
  ],
};
