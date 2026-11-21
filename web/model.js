'use strict';

const skills = [
  { id: 'fire', name: '火焰斩', element: '火系', gesture: '横挥', glyph: 'fire', type: '攻击', instruction: '向侧面横挥魔杖，划出一道火弧。', effect: '斩击目标，火焰对回春姿态有克制加成。' },
  { id: 'frost', name: '冰霜箭', element: '冰系', gesture: '前推', glyph: 'snow', type: '攻击', instruction: '握稳魔杖，沿杖尖方向向前推出。', effect: '发射冰箭，对火焰姿态造成额外伤害。' },
  { id: 'ward', name: '星鳞盾', element: '星系', gesture: '翻腕', glyph: 'shield', type: '防御', instruction: '沿魔杖长轴翻转手腕，展开星鳞。', effect: '在试炼中抵挡大部分反击，在决斗中格挡下一次攻击。' },
  { id: 'bloom', name: '回春阵', element: '自然系', gesture: '画圈', glyph: 'leaf', type: '治疗', instruction: '用杖尖沿同一方向画一个完整的圆。', effect: '恢复自己的生命，在协作副本中也能治疗队友。' }
];
const modes = {
  solo: { title: '单人试炼', foe: '裂隙守卫', art: 'boss', glyph: 'portal', tag: 'Boss 挑战', health: 100, reward: 22 },
  duel: { title: '双人决斗', foe: '镜像术士', art: 'duel', glyph: 'swords', tag: '对战预演', health: 100, reward: 12 },
  raid: { title: '协作副本', foe: '裂隙守卫', art: 'portal', glyph: 'users', tag: '组队预演', health: 145, reward: 22 }
};
const navGroups = [
  { name: '工作台', items: [['home','nest','灵巢总览'], ['training','wand','法术训练'], ['trials','portal','试炼大厅'], ['rooms','users','联机房间']] },
  { name: '魔杖', items: [['runes','rune','现实机关']] }
];
const slotTypes = [
  { id: 'light', name: '灯光', glyph: 'light' },
  { id: 'media', name: '影音', glyph: 'play' },
  { id: 'other', name: '自定义', glyph: 'rune' }
];
const petName = '霜芽';
const storageKey = 'rift.web.studio.v1';
const validCount = value => Number.isSafeInteger(value) && value >= 0 ? value : 0;
const cap = (number, min, max) => Math.max(min, Math.min(max, number));
const skillById = id => skills.find(skill => skill.id === id) || skills[1];
const petLevel = xp => 1 + Math.min(Math.floor(xp / 30), 29);
const rank = count => count === 3 ? '已掌握' : count > 0 ? '练习中' : '待练习';
let storageAvailable = true;

function loadProgress() {
  let source = {};
  try {
    const stored = JSON.parse(localStorage.getItem(storageKey) || '{}');
    if (stored && typeof stored === 'object') source = stored;
  } catch { storageAvailable = false; }
  return {
    xp: validCount(source.xp), wins: validCount(source.wins), runes: validCount(source.runes),
    mastery: skills.map((_, i) => Math.min(validCount(source.mastery?.[i]), 3)),
    sound: source.sound === true,
    slots: Array.from({ length: 3 }, (_, i) => {
      const slot = source.slots?.[i];
      if (!slot || typeof slot.name !== 'string' || !slot.name.trim()) return null;
      const type = slotTypes.find(item => item.id === slot.type) || slotTypes[2];
      return { name: slot.name.trim().slice(0, 20), type: type.id, used: validCount(slot.used) };
    }),
    log: Array.isArray(source.log) ? source.log.filter(entry => entry && typeof entry.text === 'string' && validCount(entry.at) && ['wand','trophy','rune'].includes(entry.glyph)).slice(0, 12) : []
  };
}
const progress = loadProgress();

function persist() {
  try {
    localStorage.setItem(storageKey, JSON.stringify(progress));
    storageAvailable = true;
  } catch { storageAvailable = false; }
}

function record(glyph, text) {
  progress.log.unshift({ at: Date.now(), glyph, text });
  progress.log.length = Math.min(progress.log.length, 12);
  persist();
}

function finishPractice(skill) {
  const index = skills.indexOf(skill);
  progress.mastery[index] = Math.min(3, progress.mastery[index] + 1);
  progress.xp += 2;
  record('wand', `完成${skill.name}训练，获得 2 XP`);
}

function newEncounter(mode) {
  return {
    mode, turn: 0, hp: [100, 100], boss: modes[mode].health,
    shields: [false, false], stance: ['fire', 'fire'], lines: [], result: '', awarded: false
  };
}
const omen = encounter => Math.floor(encounter.turn / 2) % 2 === 0 ? 'fire' : 'frost';
const enemyHp = encounter => encounter.mode === 'duel' ? encounter.hp[1] : encounter.boss;

function conclude(encounter, message) {
  encounter.lines.unshift(message);
  encounter.lines.length = Math.min(encounter.lines.length, 9);
  if (encounter.hp[0] === 0 || (encounter.mode === 'raid' && encounter.hp[1] === 0)) encounter.result = 'lose';
  else if (enemyHp(encounter) === 0) encounter.result = 'win';
  if (encounter.result !== 'win' || encounter.awarded) return;
  encounter.awarded = true;
  progress.wins += 1;
  progress.xp += modes[encounter.mode].reward;
  if (encounter.mode !== 'duel') progress.runes += 1;
  record('trophy', `${modes[encounter.mode].title}获胜，获得 ${modes[encounter.mode].reward} XP${encounter.mode === 'duel' ? '' : ' 和 1 枚符文'}`);
}

// Each browser turn is a local preview. Strength is fixed for the arena's tap controls.
function takeTurn(encounter, skillId, actor) {
  if (encounter.result) return;
  const other = 1 - actor;
  const power = 25;
  const duel = encounter.mode === 'duel';
  const actorName = actor === 0 ? petName : duel ? '镜像术士' : '镜像队友';
  const targetName = actor === 0 ? modes[encounter.mode].foe : duel ? petName : modes[encounter.mode].foe;
  const skill = skillById(skillId);
  encounter.stance[actor] = skillId;
  let message = `${actorName}施放${skill.name}`;
  if (skillId === 'bloom') {
    const healed = Math.min(100 - encounter.hp[actor], power);
    encounter.hp[actor] += healed;
    message += `，恢复 ${healed} 生命`;
    if (encounter.mode === 'raid') {
      const shared = Math.min(100 - encounter.hp[other], Math.floor(power / 2));
      encounter.hp[other] += shared;
      message += `，队友恢复 ${shared} 生命`;
    }
  } else if (skillId === 'ward') {
    encounter.shields[actor] = true;
    message += '，架起护盾';
  } else if (!duel) {
    const damage = power + (skillId !== omen(encounter) ? 7 : 0);
    encounter.boss = Math.max(0, encounter.boss - damage);
    message += `，守卫受到 ${damage} 点伤害`;
  } else if (encounter.shields[other]) {
    encounter.shields[other] = false;
    encounter.hp[actor] = Math.max(0, encounter.hp[actor] - 7);
    message += `，${targetName}格挡并反击 7 点伤害`;
  } else {
    const stance = encounter.stance[other];
    const bonus = (skillId === 'frost' && stance === 'fire') || (skillId === 'fire' && stance === 'bloom');
    const damage = power + (bonus ? 6 : 0);
    encounter.hp[other] = Math.max(0, encounter.hp[other] - damage);
    message += `，${targetName}受到 ${damage} 点伤害`;
  }
  if (!duel && encounter.boss > 0) {
    const counter = encounter.shields[actor] ? 3 : 10 + encounter.turn % 4 * 2;
    encounter.hp[actor] = Math.max(0, encounter.hp[actor] - counter);
    encounter.shields[actor] = false;
    message += `，守卫反击 ${counter} 点`;
  }
  encounter.turn++;
  conclude(encounter, message);
}

function castInEncounter(encounter, skillId) {
  if (!encounter || encounter.result) return;
  takeTurn(encounter, skillId, 0);
  if (encounter.result || encounter.mode === 'solo') return;
  const cycle = encounter.mode === 'duel' ? ['fire', 'ward', 'frost', 'bloom'] : ['frost', 'fire', 'bloom'];
  takeTurn(encounter, cycle[Math.floor((encounter.turn - 1) / 2) % cycle.length], 1);
}
