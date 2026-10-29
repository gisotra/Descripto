// Textos claros dos testes (escritos à mão, fora dos textos usados para treinar os modelos de idioma).
// "limpar" deixa só letras minúsculas de a a z, como o regulamento exige.
'use strict';

function limpar(texto) {
  return texto.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/[^a-z]/g, '');
}

const PORTUGUES = [
  'A cidade acordou cedo naquele sábado e as ruas já estavam cheias de gente caminhando em direção ao mercado, onde as barracas vendiam frutas frescas, pães quentes e flores de todas as cores, enquanto crianças corriam entre as mesas rindo alto.',
  'Durante a aula de ciências o professor mostrou como as plantas transformam a luz do sol em energia e pediu que cada aluno plantasse uma semente de feijão em um copo com algodão molhado para acompanhar o crescimento durante as próximas semanas.',
  'Minha avó sempre dizia que a melhor receita é aquela feita com paciência, por isso aos domingos toda a família se reunia na cozinha para preparar o almoço, conversando sobre a semana e provando o tempero antes de servir a mesa.',
  'O time treinou duro durante o inverno inteiro porque queria chegar bem preparado ao campeonato regional, e quando o juiz apitou o início da final todos os jogadores sabiam exatamente o que precisavam fazer para conquistar a vitória.',
  'Depois de muitos anos morando longe, ele finalmente voltou para a pequena cidade onde nasceu e percebeu que a praça, a igreja e a padaria continuavam no mesmo lugar, embora as pessoas tivessem mudado bastante com o passar do tempo.',
  'A biblioteca da escola recebeu novos livros de aventura e de mistério, e os estudantes formaram uma longa fila para escolher o primeiro título, prometendo ler tudo até o fim do mês e trocar opiniões durante o intervalo das aulas.',
  'Quando a chuva começou a cair forte sobre a estrada, o motorista reduziu a velocidade e ligou os faróis, lembrando das recomendações de segurança que aprendera, e assim conseguiu chegar em casa sem nenhum susto naquela noite fria.',
  'Os alunos do terceiro ano organizaram uma feira de projetos onde cada grupo apresentou uma invenção criada com materiais recicláveis, e os visitantes puderam votar na ideia mais criativa, que ganhou um prêmio simbólico no final da tarde.',
  'No fim da tarde o sol desce devagar atrás das montanhas e o céu fica cor de laranja, enquanto os pássaros voltam para os ninhos e os pescadores recolhem as redes, deixando o rio calmo e silencioso até a manhã seguinte.',
  'Para participar do torneio cada grupo precisa desenvolver a própria ferramenta de cifra, testar com cuidado todas as chaves possíveis e entregar o desafio ao professor, que confere se o texto cifrado corresponde às regras da fase.',
  'Ela guardou a carta dentro de uma caixa de madeira junto com fotografias antigas e algumas moedas, pensando que um dia seus netos iriam encontrar aquelas lembranças e descobrir como era a vida de sua família muitos anos atrás.',
  'O museu da cidade abriu uma nova exposição sobre a história da navegação, com mapas antigos, bússolas e modelos de navios, e os visitantes podem ouvir relatos de marinheiros que atravessaram oceanos em busca de novas terras e riquezas.',
].map(limpar);

const INGLES = [
  'Yesterday the neighbours organised a small street party where everyone brought homemade dishes, folding chairs and old board games, and by the evening the children were running between the tables while the adults argued cheerfully about the rules of cards.',
  'The committee reviewed the proposal for the new library and agreed that the budget was reasonable, although several members asked for more detailed plans about the opening hours, the staff training and the maintenance of the reading rooms.',
  'Walking along the coast early in the morning, she noticed that the tide had left a long line of shells, seaweed and smooth stones, and she stopped several times to photograph the patterns before the waves washed them away again.',
  'After months of preparation the team finally released the software update, which fixed many reported bugs, improved the speed of the search function and added a simple dark theme that users had been requesting for a long time.',
  'My grandfather used to say that a good story needs three things, a curious beginning, a difficult middle and an ending that makes the listener want to hear another one, and he always told them slowly beside the fire.',
  'The museum opened a new exhibition about ancient maps and navigation, showing compasses, globes and handwritten charts, and visitors can listen to recordings of sailors describing storms, islands and the long weeks without seeing land.',
  'Because the train was delayed by heavy snow, the passengers waited in the small station cafe, sharing newspapers, drinking hot chocolate and telling jokes, until an announcement finally said that the tracks had been cleared.',
  'In the science class the teacher explained how plants turn sunlight into energy and asked every student to grow a bean seed in a glass jar so that they could observe the roots and the first leaves during the following weeks.',
].map(limpar);

// Texto de exemplo do enunciado (inglês), cifrado com César(3) e depois Vigenère("abc").
const EXEMPLO_CLARO = limpar(
  'The quick brown fox jumps over the lazy dog. By the Toston Journal in the beginning the universe was created. ' +
    'This had made many people very angry and has been widely regarded as a bad move. By Douglas Adams',
);
const EXEMPLO_CIFRADO =
  'wljtynfogusbqjtanzptxrzjuxmhpfccirkgbxmhxtvxtqntxvsdpnqxmhfjjmsqmsjxmhyslzjuwjzexfvjdxjgxmlwmdhrdhjpesbtjrtqhzjucfqkwbesglfvfjhrblhjocwhkfuhjgexdffgqtyigbhtxkqdwfgerv';

// Textos em português com MUITOS nomes próprios estrangeiros (escalações, enxadristas): a média de trigramas do texto
// em claro fica abaixo do limiar de texto natural, como no lyr.txt da fase de testes (seleção georgiana).
const NOMES_PROPRIOS = [
  'A escalação da seleção georgiana é Mamardashvili, Kakabadze, Dvali, Lochoshvili, Sazonov, Kochorashvili, Chakvetadze, ' +
    'Kvaratskhelia, Mikautadze, Davitashvili e Zivzivadze; o técnico Willy Sagnol escalou o time com três defensores para ' +
    'enfrentar Portugal na Euro.',
  'O polonês Szczesny jogou longe de casa enquanto Lewandowski, Zielinski e Krychowiak marcaram gols contra a seleção da ' +
    'Eslováquia em Varsóvia; os jornalistas Brzeczek e Przybylski elogiaram a atuação do goleiro Wojciech na partida de ontem.',
  'No torneio de xadrez de Moscou os jogadores Nepomniachtchi e Karjakin venceram Kramnik e Grischuk, enquanto Dubov e ' +
    'Svidler empataram todas as partidas da tarde e os organizadores Zhukovsky e Tkachenko anunciaram a final para domingo.',
].map(limpar);

module.exports = { limpar, PORTUGUES, INGLES, EXEMPLO_CLARO, EXEMPLO_CIFRADO, NOMES_PROPRIOS };
