// Compila c/motor.c em bin/motor (bin/motor.exe no Windows). Chamado pelo "npm run electron".
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const raiz = path.join(__dirname, '..');
const pastaBin = path.join(raiz, 'bin');
const fonte = path.join(raiz, 'c', 'motor.c');
const saida = path.join(pastaBin, process.platform === 'win32' ? 'motor.exe' : 'motor');

fs.mkdirSync(pastaBin, { recursive: true });

try {
  execFileSync('gcc', ['-O2', '-Wall', '-Wextra', '-std=gnu11', '-o', saida, fonte, '-lm'], {
    stdio: 'inherit',
  });
  console.log(`Motor em C compilado: ${path.relative(raiz, saida)}`);
} catch (erro) {
  if (erro.code === 'ENOENT') {
    console.error(
      process.platform === 'win32'
        ? 'gcc não encontrado. Instale o MinGW-w64 (ex.: pelo MSYS2) e coloque o gcc no PATH.'
        : 'gcc não encontrado. Instale com: sudo apt install build-essential',
    );
  } else {
    console.error('Falha ao compilar c/motor.c (veja as mensagens do gcc acima).');
  }
  process.exit(1);
}
