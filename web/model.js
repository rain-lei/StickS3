'use strict';

const skills = [
  { id: 'fire', name: '火焰斩', element: '火系', gesture: '横挥', glyph: 'fire', type: '攻击', instruction: '向侧面横挥魔杖，划出一道火弧。', effect: '斩击目标。决斗中火斩冷却 2.4 秒，适合追击。' },
  { id: 'frost', name: '冰霜箭', element: '冰系', gesture: '前推', glyph: 'snow', type: '攻击', instruction: '握稳魔杖，沿杖尖方向向前推出。', effect: '发射冰箭。决斗中命中会延缓对方攻击 0.8 秒。' },
  { id: 'ward', name: '星鳞盾', element: '星系', gesture: '翻腕', glyph: 'shield', type: '防御', instruction: '沿魔杖长轴翻转手腕，展开星鳞。', effect: '在试炼中抵挡大部分反击，在决斗中最多叠三层，每层格挡一次并反击。' },
  { id: 'bloom', name: '回春阵', element: '自然系', gesture: '画圈', glyph: 'leaf', type: '治疗', instruction: '用杖尖沿同一方向画一个完整的圆。', effect: '恢复自己的生命，在协作副本中也能治疗队友。' }
];
const modes = {
  solo: { title: '单人试炼', foe: '裂隙守卫', art: 'boss', glyph: 'portal', tag: 'Boss 挑战', health: 100, reward: 22 },
  duel: { title: '双人决斗', foe: '镜像术士', art: 'duel', glyph: 'swords', tag: '对战预演', health: 100, reward: 12 },
  raid: { title: '协作副本', foe: '裂隙守卫', art: 'portal', glyph: 'users', tag: '组队预演', health: 145, reward: 22 }
};
const navGroups = [
  { name: '工作台', items: [['home','nest','灵巢总览'], ['training','wand','法术训练'], ['trials','portal','试炼大厅'], ['rooms','users','联机房间'], ['watch','device','实机观战']] },
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
    clock: 0, nextOpponent: 1300, projectiles: [],
    readyAt: [{}, {}], shields: [0, 0], lines: [], result: '', awarded: false
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

const duelCooldown = {fire:2400, frost:3600, ward:4200, bloom:7500};
function duelAvailable(e, id, actor) {
  return !e.result && (e.readyAt[actor][id] || 0) <= e.clock &&
    (id !== 'ward' || e.shields[actor] < 3) && (id !== 'bloom' || e.hp[actor] < 100);
}

function duelCast(e, id, actor) {
  if (!duelAvailable(e, id, actor)) return false;
  const who = actor ? modes.duel.foe : petName;
  e.readyAt[actor][id] = e.clock + duelCooldown[id];
  if (id === 'ward') {
    e.shields[actor]++;
    conclude(e, `${who}展开星鳞盾 · ${e.shields[actor]} 层`);
  } else if (id === 'bloom') {
    e.hp[actor] = Math.min(100, e.hp[actor] + 24);
    conclude(e, `${who}施放回春阵，生命恢复至 ${e.hp[actor]}`);
  } else {
    e.projectiles.push({actor, id, lands:e.clock + (id === 'fire' ? 650 : 950)});
    conclude(e, `${who}释放${skillById(id).name}`);
  }
  return true;
}

// The mirror decides on its own clock. An incoming spell leaves time to raise a ward.
function advanceDuel(e, dt, random = Math.random) {
  if (e.result) return false;
  e.clock += dt;
  let changed = false;
  e.projectiles.sort((a,b) => a.lands-b.lands);
  while (!e.result && e.projectiles.length && e.projectiles[0].lands <= e.clock) {
    const {actor,id} = e.projectiles.shift();
    const other = 1-actor;
    const target = other ? modes.duel.foe : petName;
    if (e.shields[other]) {
      e.shields[other]--;
      e.hp[actor] = Math.max(0,e.hp[actor]-7);
      conclude(e, `${target}消耗一层盾，反击 7 点`);
    } else {
      const damage = id === 'fire' ? 24 : 30;
      e.hp[other] = Math.max(0, e.hp[other]-damage);
      if (id==='frost') {
        for (const attack of ['fire','frost']) e.readyAt[other][attack]=Math.max(e.readyAt[other][attack] || 0,e.clock+800);
      }
      conclude(e, `${skillById(id).name}命中${target}，造成 ${damage} 点伤害`);
    }
    changed = true;
  }
  if (!e.result && e.clock >= e.nextOpponent) {
    const choices = ['fire','frost','ward'];
    if (e.hp[1]<80) choices.push('bloom','bloom');
    if (e.projectiles.some(shot => shot.actor===0)) choices.push('ward','ward');
    const available = choices.filter(id => duelAvailable(e,id,1));
    if (available.length) changed = duelCast(e,available[Math.floor(random()*available.length)],1) || changed;
    e.nextOpponent = e.clock + 850 + random()*1450;
  }
  return changed;
}

function takeTurn(encounter, skillId, actor) {
  if (encounter.result) return;
  const other = 1 - actor;
  const power = 25;
  const actorName = actor === 0 ? petName : '镜像队友';
  const skill = skillById(skillId);
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
  } else {
    const damage = power + (skillId !== omen(encounter) ? 7 : 0);
    encounter.boss = Math.max(0, encounter.boss - damage);
    message += `，守卫受到 ${damage} 点伤害`;
  }
  if (encounter.boss > 0) {
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
  if (encounter.mode === 'duel') return duelCast(encounter, skillId, 0);
  takeTurn(encounter, skillId, 0);
  if (encounter.result || encounter.mode === 'solo') return;
  const cycle = ['frost', 'fire', 'bloom'];
  takeTurn(encounter, cycle[Math.floor((encounter.turn - 1) / 2) % cycle.length], 1);
}
