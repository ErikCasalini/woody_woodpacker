import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import cyberdream from './src/themes/cyberdream.mjs';
import terminal from './src/themes/terminal.mjs';

export default defineConfig({
  site: 'https://erikcasalini.github.io',
  base: '/woody_woodpacker',
  integrations: [
    starlight({
      title: 'Woody-Woodpacker',
      favicon: '/favicon.png',
      customCss: [
        '@fontsource-variable/jetbrains-mono',
        '@fontsource-variable/jetbrains-mono/wght-italic.css',
        './src/styles/layout.css',
        './src/styles/cyberdream.css',
        './src/styles/terminal.css',
      ],
      routeMiddleware: './src/write-up-layout.ts',
      components: {
        ThemeProvider: './src/components/ThemeProvider.astro',
        ThemeSelect: './src/components/ThemeSelect.astro',
      },
      expressiveCode: {
        themes: [cyberdream, 'starlight-light', terminal],
        useDarkModeMediaQuery: false,
      },
      defaultLocale: 'root',
      locales: {
        root: { label: 'Français', lang: 'fr' },
        en: { label: 'English', lang: 'en' },
      },
      social: [
        {
          icon: 'github',
          label: 'GitHub',
          href: 'https://github.com/ErikCasalini/woody_woodpacker',
        },
      ],
      tableOfContents: { minHeadingLevel: 2, maxHeadingLevel: 4 },
    }),
  ],
});
