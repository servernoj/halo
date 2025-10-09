import { defineConfig } from 'vite'
import nodeExternals from 'rollup-plugin-node-externals'
import { fileURLToPath, URL } from 'node:url'

export default defineConfig({
  plugins: [
    nodeExternals()
  ],
  json: {
    namedExports: false
  },
  build: {
    target: 'esnext',
    outDir: 'dist',
    lib: {
      formats: ['es'],
      entry: {
        index: fileURLToPath(
          new URL(
            'src/index.ts',
            import.meta.url
          )
        )
      }
    }
  },
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url))
    }
  }
})
