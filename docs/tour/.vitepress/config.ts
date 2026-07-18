import { defineConfig } from 'vitepress'
import { withMermaid } from 'vitepress-plugin-mermaid'

export default withMermaid(
  defineConfig({
    title: 'Hyperverse',
    description: 'Architecture notes for the Hyperverse vertical slice.',
    base: '/hyperverse/',
    themeConfig: {
      nav: [
        { text: 'Guide', link: '/' },
        { text: 'Events', link: '/event-reference' },
      ],
      sidebar: [
        {
          text: 'Guided Tour',
          items: [
            { text: 'Overview', link: '/' },
            { text: 'Architecture', link: '/architecture' },
            { text: 'Development', link: '/development' },
            { text: 'Glossary', link: '/glossary' },
          ],
        },
        {
          text: 'Gameplay Spine',
          items: [
            { text: 'Event Flow', link: '/event-flow' },
            { text: 'Event Reference', link: '/event-reference' },
            { text: 'Context Objects', link: '/context-objects' },
            { text: 'State Machines', link: '/state-machines' },
          ],
        },
        {
          text: 'Libraries',
          items: [
            { text: 'Boost.Ext SML', link: '/libraries/boost_ext_sml' },
            { text: 'EnTT', link: '/libraries/entt' },
            { text: 'EventPP', link: '/libraries/eventpp' },
          ],
        },
      ],
      socialLinks: [
        { icon: 'github', link: 'https://github.com/meleneth/hyperverse' },
      ],
    },
    mermaid: {},
  })
)
