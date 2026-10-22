// Ponte segura entre a tela (renderer) e o processo principal.
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('cripto', {
  executar: (pedido) => ipcRenderer.invoke('cripto:executar', pedido),
  abrirTxt: () => ipcRenderer.invoke('cripto:abrir-txt'),
  salvar: (conteudo) => ipcRenderer.invoke('cripto:salvar', conteudo),
  copiar: (conteudo) => ipcRenderer.invoke('cripto:copiar', conteudo),
  codigos: () => ipcRenderer.invoke('cripto:codigos'),
});
