import { resolve } from 'path';
import { defineConfig } from 'vite';

export default defineConfig({
  // A pasta web será a raiz do servidor de desenvolvimento e do build
  root: 'web',
  build: {
    // A pasta de saída será na raiz do repositório (fora da pasta web)
    outDir: '../dist',
    // Limpa a pasta dist antes de cada build
    emptyOutDir: true,
    rollupOptions: {
      input: {
        main: resolve(import.meta.dirname, 'web/index.html'),
        admin: resolve(import.meta.dirname, 'web/admin.html')
      }
    }
  }
});
