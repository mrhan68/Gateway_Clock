/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  darkMode: 'class',
  theme: {
    extend: {
      fontFamily: {
        sans: [
          'system-ui',
          '-apple-system',
          'BlinkMacSystemFont',
          '"Segoe UI"',
          'Roboto',
          'sans-serif',
        ],
        mono: [
          '"JetBrains Mono"',
          'ui-monospace',
          'SFMono-Regular',
          'Menlo',
          'monospace',
        ],
      },
      colors: {
        primary: '#3b82f6',
        secondary: '#10b981',
        danger: '#ef4444',
        warning: '#f59e0b',
        'gateway-online': '#10b981',
        'gateway-offline': '#ef4444',
        zinc: {
          750: '#303036',
        },
        // Dark palette tokens (Linear-style)
        dark: {
          950: '#09090b', // Base background
          900: '#121215', // Card background
          850: '#18181b', // Card hover / elevated
          800: '#27272a', // Borders / dividers
          700: '#3f3f46', // Subtle text / muted borders
        },
      },
      boxShadow: {
        'glow-emerald': '0 0 20px -3px rgba(16, 185, 129, 0.3)',
        'glow-blue': '0 0 20px -3px rgba(59, 130, 246, 0.3)',
        'glow-rose': '0 0 20px -3px rgba(244, 63, 94, 0.3)',
      },
    },
  },
  plugins: [],
}
