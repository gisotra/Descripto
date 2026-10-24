// Hash do código-fonte do motor (c/motor.c + c/modelo_pt.h), sem os \r (o Git no Windows pode
// trocar \n por \r\n). Usado pelo build-c.js para saber se o motor.exe pré-compilado está em dia.
//
// Depois de compilar um novo prebuilt/motor-windows-x64.exe, grave o hash com:
//   node scripts/hash-fonte.js
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const raiz = path.join(__dirname, '..');
const arquivoHash = path.join(raiz, 'prebuilt', 'motor-windows-x64.exe.fonte.sha256');

function hashDoFonte() {
  const dados = Buffer.concat([
    fs.readFileSync(path.join(raiz, 'c', 'motor.c')),
    fs.readFileSync(path.join(raiz, 'c', 'modelo_pt.h')),
  ]);
  const semCR = Buffer.from(dados.filter((byte) => byte !== 0x0d));
  return crypto.createHash('sha256').update(semCR).digest('hex');
}

if (require.main === module) {
  fs.writeFileSync(arquivoHash, `${hashDoFonte()}\n`);
  console.log(`Hash gravado em ${path.relative(raiz, arquivoHash)}: ${hashDoFonte()}`);
}

module.exports = { hashDoFonte, arquivoHash };
