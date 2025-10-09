import globals from 'globals'
import pluginJs from '@eslint/js'
import tseslint from 'typescript-eslint'
import standard from './eslint-standard.js'
import react from './eslint-react.js'

export default [
  pluginJs.configs.recommended,
  {
    files: ['**/*.{js,mjs,cjs,ts,tsx}'],
    languageOptions: {
      globals: {
        ...globals.browser,
        ...globals.node
      },
      parserOptions: {
        parser: tseslint.parser
      }
    }
  },
  ...tseslint.configs.recommended,
  ...react,
  ...standard,
  {
    rules: {
      '@typescript-eslint/no-explicit-any': 'off',
      '@typescript-eslint/no-unused-vars': 'warn',
      '@typescript-eslint/no-unsafe-function-type': 'warn'
    }
  },
  {
    ignores: [
      '**/dist/*',
      '**/node_modules/*',
      '**/artifacts/**/*.d.ts',
      '**/vite-env.d.ts'
    ]
  }
]
