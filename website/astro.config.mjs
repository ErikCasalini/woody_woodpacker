import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  site: 'https://erikcasalini.github.io',
  base: '/woody_woodpacker',
  integrations: [
    starlight({
      title: 'Woody',
      favicon: '/favicon.png',
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
