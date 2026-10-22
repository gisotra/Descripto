// Compila c/motor.c em bin/motor (chamado pelo "npm run electron").
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const raiz = path.join(__dirname, '..');
const pastaBin = path.join(raiz, 'bin');
const fonte = path.join(raiz, 'c', 'motor.c');
const saida = path.join(pastaBin, 'motor');

fs.mkdirSync(pastaBin, { recursive: true });

try {
  execFileSync('gcc', ['-O2', '-Wall', '-Wextra', '-std=c11', '-o', saida, fonte, '-lm'], {
    stdio: 'inherit',
  });
  console.log('Motor em C compilado: bin/motor');
} catch (erro) {
  if (erro.code === 'ENOENT') {
    console.error('gcc nao encontrado. Instale com: sudo apt install build-essential');
  } else {
    console.error('Falha ao compilar c/motor.c (veja as mensagens do gcc acima).');
  }
  process.exit(1);
}
