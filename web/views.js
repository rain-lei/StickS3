'use strict';

const $ = selector => document.querySelector(selector);
const esc = value => String(value).replace(/[&<>"']/g, char => ({ '&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;' }[char]));
const art = (id, className = '') => `<img src="assets/${id}.webp" alt="" class="${className}" draggable="false">`;
const navUrl = (route, detail = '') => `#/${route}${detail ? '/' + detail : ''}`;
const linkBtn = (url, label, glyph, style = 'btn primary') => `<a class="${style}" href="${url}">${glyph ? icon(glyph) : ''}${label}</a>`;
const actionBtn = (action, label, glyph, style = 'btn primary', attrs = '') => `<button type="button" class="${style}" data-act="${action}" ${attrs}>${glyph ? icon(glyph) : ''}${label}</button>`;
const sectionTitle = (title, action = '') => `<div class="section-heading"><h2>${title}</h2>${action}</div>`;
const pageTitle = (title, description, action = '') => `<div class="page-heading"><div><h1>${title}</h1><p>${description}</p></div>${action}</div>`;
const masteryBars = n => `<span class="mastery-bars" aria-hidden="true">${[1,2,3].map(i => `<i class="${i <= n ? 'filled' : ''}"></i>`).join('')}</span>`;
const countMastered = () => progress.mastery.filter(value => value === 3).length;

function xpMeter() {
  const level = petLevel(progress.xp);
  const value = level === 30 ? 30 : progress.xp % 30;
  const label = level === 30 ? '等级已满' : `${value} / 30 XP`;
  return `<div class="xp"><div><span>成长经验</span><b>${label}</b></div><div class="track" role="meter" aria-label="本级成长经验" aria-valuemin="0" aria-valuemax="30" aria-valuenow="${value}"><i style="width:${value / 30 * 100}%"></i></div></div>`;
}

function homeView() {
  const encounters = [
    ['duel','swords','双人决斗','和镜像术士练习攻防','#/rooms/duel'],
    ['portal','users','协作副本','与镜像队友挑战共享 Boss','#/rooms/raid'],
    ['shrine','rune','现实机关','为三个机关槽绑定动作','#/runes']
  ];
  return `${pageTitle('灵巢总览','照看你的精灵，选择下一场冒险。',linkBtn('#/training','开始训练','wand'))}
    <div class="spotlight">
      <article class="pet-hero">
        ${art('frostling','cover')}
        <div class="hero-shade"></div>
        <div class="pet-copy">
          <span class="image-tag pet-tag">${icon('sparkle')}我的精灵</span>
          <div class="pet-title"><h2>${petName}</h2><span class="level">Lv. ${String(petLevel(progress.xp)).padStart(2,'0')}</span></div>
          <p class="pet-element">${icon('snow')}冰霜系<span>·</span>幼龙</p>
          ${xpMeter()}
          ${actionBtn('pet','精灵档案','book','btn glass')}
        </div>
      </article>
      <article class="featured">
        ${art('boss','cover')}<div class="encounter-shade"></div>
        <div class="feature-content"><span class="image-tag boss-tag">${icon('swords')}单人试炼</span>
          <div class="feature-bottom"><h2>裂隙守卫</h2><p>击败守卫，获得现实符文。</p>
            <a href="#/trials" class="feature-link">进入试炼${icon('arrow')}</a></div>
        </div>
      </article>
    </div>
    <section class="home-section">
      ${sectionTitle('法术手册',`<a class="text-link" href="#/training">前往训练${icon('chevron')}</a>`)}
      <div class="skill-grid">${skills.map((s,i) => `<a class="skill-card ${s.id}" href="#/training/${s.id}" aria-label="训练${s.name}，动作${s.gesture}，熟练度 ${progress.mastery[i]}/3">
        <div class="skill-card-art">${art(s.id)}</div><div class="skill-copy"><span class="skill-kind">${s.element} · ${s.type}</span><h3>${s.name}</h3><span class="gesture">${icon('hand')}${s.gesture}</span></div>
        <div class="skill-status">${masteryBars(progress.mastery[i])}<span>熟练 ${progress.mastery[i]}/3</span></div></a>`).join('')}</div>
    </section>
    <section class="home-section">
      ${sectionTitle('冒险入口')}
      <div class="adventure-grid">${encounters.map(([img,glyph,name,desc,href])=>`<a class="adventure-card" href="${href}">
        ${art(img,'cover')}<div class="adventure-shade"></div><span class="adventure-icon">${icon(glyph)}</span>
        <div class="adventure-copy"><h3>${name}</h3><p>${desc}</p></div><span class="round-arrow">${icon('arrow')}</span></a>`).join('')}</div>
    </section>`;
}

function trainingView(skill, feedback = '') {
  const idx = skills.indexOf(skill);
  return `${pageTitle('法术训练','用鼠标、触屏或键盘模拟魔杖蓄力。成功释放一次，霜芽获得 2 XP。')}
    <div class="training-layout">
      <section class="panel skill-list-panel">${sectionTitle('选择法术')}<div class="skill-list">
        ${skills.map((s,i)=>`<a class="skill-option ${s.id} ${s===skill?'selected':''}" href="#/training/${s.id}" ${s===skill?'aria-current="true"':''}>
          <span class="option-art">${art(s.id)}</span><span class="option-label"><strong>${s.name}</strong><small>${s.gesture} · ${s.type}</small></span><span class="option-mastery">${progress.mastery[i]}/3</span></a>`).join('')}
      </div></section>
      <section class="panel practice-panel ${skill.id}">
        <div class="practice-head"><span class="small-chip tone">${icon(skill.glyph)}${skill.element} · ${skill.type}</span><span class="muted">${rank(progress.mastery[idx])} ${progress.mastery[idx]}/3</span></div>
        <div class="spell-orbit">${art(skill.id)}<svg class="charge-ring" viewBox="0 0 120 120" aria-hidden="true"><circle cx="60" cy="60" r="56" pathLength="100"/></svg></div>
        <h2>${skill.name}</h2><p>${skill.instruction}</p>
        <div class="casting-controls">
          <div class="energy-meta"><span>施法能量</span><b id="energy-number">0%</b></div>
          <div class="track energy" role="meter" aria-label="施法能量" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100"><i id="energy-fill"></i></div>
          <button type="button" class="btn cast-button" id="cast-button" aria-describedby="cast-tip">${icon('wand')}<span>按住蓄力，松开释放</span></button>
          <small id="cast-tip">蓄力过半即可释放，键盘可按住空格。</small>
          <p id="practice-feedback" class="practice-feedback" role="status">${esc(feedback)}</p>
        </div>
      </section>
    </div>
    <section class="panel hint-panel">${icon('book')}<div><h2>施法笔记</h2><p>${skill.effect} 每种法术完成三次训练即可掌握。</p></div></section>`;
}

function trialsView() {
  return `${pageTitle('试炼大厅','观察守卫的属性预兆，选择克制它的法术。')}
    <article class="mode-banner solo-banner">${art('boss','cover')}<div class="mode-shade"></div>
      <div class="mode-copy"><span class="image-tag boss-tag">${icon('portal')}单人试炼</span><h2>裂隙守卫</h2>
        <p>守护这片熔岩的裂隙龙已经苏醒。护盾能减轻反击，回春阵可以恢复霜芽的生命。</p>
        ${actionBtn('battle-start','开始挑战','swords','btn primary','data-mode="solo"')}</div>
    </article>
    <div class="fact-grid"><article class="panel fact">${icon('heart')}<div><small>守卫生命</small><strong>100 HP</strong></div></article>
      <article class="panel fact">${icon('sparkle')}<div><small>胜利经验</small><strong>+22 XP</strong></div></article>
      <article class="panel fact">${icon('rune')}<div><small>通关奖励</small><strong>现实符文 × 1</strong></div></article></div>
    <section class="panel hint-panel">${icon('info')}<div><h2>本地试炼规则</h2><p>点击法术即结算一回合。守卫仍有生命时会反击，使用不同于预兆的攻击属性可多造成 7 点伤害。</p></div></section>`;
}

function roomsView(mode) {
  const raid = mode === 'raid';
  const item = modes[mode];
  return `${pageTitle('联机房间','选择一个模式，用镜像角色预演双人回合。')}
    <div class="mode-tabs" aria-label="房间模式"><a class="${!raid?'active':''}" ${!raid?'aria-current="page"':''} href="#/rooms/duel">${icon('swords')}双人决斗</a><a class="${raid?'active':''}" ${raid?'aria-current="page"':''} href="#/rooms/raid">${icon('users')}协作副本</a></div>
    <article class="mode-banner room-banner">${art(item.art,'cover')}<div class="mode-shade"></div><div class="mode-copy">
      <span class="image-tag pet-tag">${icon('users')}双人预演</span><h2>${item.title}</h2><p>${raid?'和镜像队友轮流施法，共同击败拥有 145 点生命的裂隙守卫。':'和镜像术士交替行动，练习冰火克制与星鳞盾的格挡反击。'}</p>
      ${actionBtn('battle-start',raid?'开始副本预演':'开始决斗预演',item.glyph,'btn primary',`data-mode="${mode}"`)}</div></article>
    <section class="panel party-panel">${sectionTitle('出场成员')}<div class="party-line"><span class="party-avatar frost">${icon('snow')}</span><div><strong>霜芽</strong><small>你操控的精灵</small></div><span class="small-chip">玩家</span></div>
      <div class="party-line"><span class="party-avatar purple">${icon(raid?'shield':'swords')}</span><div><strong>${raid?'镜像队友':'镜像术士'}</strong><small>${raid?'协助攻击与治疗':'自动出招与格挡'}</small></div><span class="small-chip">浏览器模拟</span></div></section>
    ${raid?'':`<section class="panel hint-panel">${icon('target')}<div><h2>双杖红外判定</h2><p>本地预演假定红外畅通。真机放出火焰斩或冰霜箭后，两杖的红外窗要正对至结算。守方可提前翻腕格挡，未命中时可在自己的屏幕查看接收提示。</p></div></section>`}`;
}

function healthMeter(label, value, max, color) {
  return `<div class="health ${color}"><div><strong>${label}</strong><b>${value} / ${max}</b></div><div class="track" role="meter" aria-label="${label}生命" aria-valuemin="0" aria-valuemax="${max}" aria-valuenow="${value}"><i style="width:${value/max*100}%"></i></div></div>`;
}

function arenaView(battle) {
  const item = modes[battle.mode];
  const raid = battle.mode === 'raid';
  const duel = battle.mode === 'duel';
  const back = duel || raid ? `#/rooms/${battle.mode}` : '#/trials';
  const won = battle.result === 'win';
  const status = battle.result ? (won?'试炼胜利':'本场结束') : `第 ${duel||raid?Math.floor(battle.turn/2)+1:battle.turn+1} 回合`;
  return `${pageTitle(item.title,duel?'与镜像术士交锋，点击法术执行本回合的动作。':raid?'镜像队友会在霜芽行动后自动施法。':'轮到霜芽行动，选择本回合的法术。',linkBtn(back,'返回大厅','back','btn secondary'))}
    <div class="arena-layout"><section class="panel arena-main">
      <div class="arena-stage">${art(item.art,'cover')}<div class="arena-shade"></div><span class="image-tag pet-tag">${icon(item.glyph)}${status}</span><div class="opponent"><small>${duel?'模拟对手':'试炼 Boss'}</small><h2>${item.foe}</h2></div></div>
      <div class="arena-stats">${healthMeter(item.foe,enemyHp(battle),item.health,'danger')}${healthMeter(petName,battle.hp[0],100,'vital')}${raid?healthMeter('镜像队友',battle.hp[1],100,'magic'):''}</div>
      ${battle.result ? `<div class="settlement"><h3>${won?'胜利！':'精灵力竭'}</h3><p>${won?`霜芽获得 ${item.reward} XP${duel?'':' 和 1 枚现实符文'}。`:'霜芽已回到灵巢休整，可以重新挑战。'}</p>${actionBtn('battle-start','再来一场',item.glyph,'btn primary',`data-mode="${battle.mode}"`)}${linkBtn('#/home','回到灵巢',null,'btn secondary')}</div>` : `<div class="spell-actions"><div class="section-heading"><h2>选择法术</h2>${duel?'':`<span class="small-chip tone ${omen(battle)}">${icon(skillById(omen(battle)).glyph)}${skillById(omen(battle)).element}预兆</span>`}</div>
      <div class="battle-skills">${skills.map(s=>`<button type="button" data-act="battle-cast" data-skill="${s.id}" class="battle-skill ${s.id}">${art(s.id)}<span>${s.name}</span></button>`).join('')}</div></div>`}
    </section><section class="panel battle-history">${sectionTitle('战斗记录')}<ol class="timeline">${battle.lines.length?battle.lines.map((message,i)=>`<li class="${i===0?'fresh':''}">${icon('bolt')}<span>${esc(message)}</span></li>`).join(''):`<li>${icon('portal')}<span>霜芽进入${item.title}，等待第一次施法。</span></li>`}</ol></section></div>`;
}

function runesView() {
  return `${pageTitle('现实机关','把一个遥控动作绑定到一个槽位，预演霜芽触发机关的效果。')}
    <article class="mode-banner rune-banner">${art('shrine','cover')}<div class="mode-shade"></div><div class="mode-copy"><span class="image-tag green-tag">${icon('rune')}符文收藏</span>
      <h2>现实符文 <span class="rune-total">× ${progress.runes}</span></h2><p>三个槽位均可本地预演。真机施放需要已学习的遥控码，以及通过单人试炼或协作副本获得的符文。</p></div></article>
    <section class="home-section">${sectionTitle('机关槽位',`<span class="muted">${progress.slots.filter(Boolean).length} / 3 已绑定</span>`)}<div class="slot-grid">${progress.slots.map((slot,i)=>{
      const type = slotTypes.find(t=>t.id===slot?.type);
      return `<article class="panel slot ${slot?'bound':''}"><div class="slot-top"><span class="slot-symbol">${icon(type?.glyph||'link')}</span><small>槽位 0${i+1}</small></div><h3>${slot?esc(slot.name):'空槽位'}</h3><p>${slot?`${type.name} · 已预演 ${slot.used} 次`:'可绑定一个遥控动作'}</p><div class="slot-actions">${slot?`${actionBtn('slot-trigger','预演触发','play','btn compact primary',`data-index="${i}"`)}${actionBtn('slot-edit','管理',null,'btn compact secondary',`data-index="${i}"`)}`:actionBtn('slot-edit','绑定动作','plus','btn compact secondary',`data-index="${i}"`)}</div></article>`;
    }).join('')}</div></section>`;
}

function dialogHeading(glyph, title, subtitle = '') {
  return `<span class="dialog-symbol">${icon(glyph)}</span><h2 id="dialog-title">${title}</h2>${subtitle?`<p class="dialog-intro">${subtitle}</p>`:''}`;
}
function petDialog() {
  return `<div class="modal-portrait">${art('frostling','cover')}</div><div class="modal-pad pet-dialog"><span class="small-chip tone frost">${icon('snow')}冰霜系幼龙</span><h2 id="dialog-title">${petName}<small>Lv. ${String(petLevel(progress.xp)).padStart(2,'0')}</small></h2>${xpMeter()}
    <div class="pet-facts"><div><b>${countMastered()} / 4</b><small>已掌握法术</small></div><div><b>${progress.wins}</b><small>试炼与决斗胜场</small></div><div><b>${progress.runes}</b><small>现实符文</small></div></div>
    <h3>最近足迹</h3>${progress.log.length?`<ul class="pet-log">${progress.log.slice(0,4).map(entry=>`<li>${icon(entry.glyph)}<span>${esc(entry.text)}</span></li>`).join('')}</ul>`:'<p class="muted">霜芽刚刚醒来。完成第一次训练，就会在这里留下足迹。</p>'}</div>`;
}
function deviceDialog() {
  return `<div class="modal-pad">${dialogHeading('device','魔杖状态','网页演练进度保存在当前浏览器。两台魔杖通过 ESP-NOW 结算真机对战，结果显示在各自的板载屏幕上。')}
    <dl class="detail-rows"><div><dt>当前模式</dt><dd>本地演练</dd></div><div><dt>StickS3</dt><dd>未连接</dd></div><div><dt>红外收发</dt><dd>未接入</dd></div><div><dt>演练存档</dt><dd>${storageAvailable?'当前浏览器':'仅本次会话'}</dd></div></dl>${actionBtn('close','知道了',null,'btn primary wide')}</div>`;
}
function helpDialog() {
  return `<div class="modal-pad">${dialogHeading('book','游玩指南','从一次施法开始，霜芽的每次成长都会记录在灵巢。')}
    <ol class="help-list"><li>${icon('wand')}<div><strong>练习法术</strong><p>选择法术，按住蓄力到 50% 后松开。成功三次即可掌握该法术。</p></div></li><li>${icon('swords')}<div><strong>进入战斗</strong><p>点击技能即可结算回合，在试炼大厅挑战守卫，或到联机房间预演双人玩法。</p></div></li><li>${icon('rune')}<div><strong>预演机关</strong><p>为槽位命名并选择设备类型。触发时会显示模拟反馈并记录次数。</p></div></li></ol>${actionBtn('close','回到工作台',null,'btn primary wide')}</div>`;
}
function slotDialog(index) {
  const slot = progress.slots[index];
  return `<form class="modal-pad" id="slot-form" data-index="${index}">${dialogHeading('link',`${slot?'管理':'绑定'}机关 · 槽位 0${index+1}`,'为这个模拟遥控动作起一个能认出的名字。')}
    <label class="field">动作名称<input name="name" required maxlength="20" autocomplete="off" placeholder="点亮书桌灯" value="${esc(slot?.name||'')}"></label>
    <label class="field">设备类型<select name="type">${slotTypes.map(t=>`<option value="${t.id}" ${slot?.type===t.id?'selected':''}>${t.name}</option>`).join('')}</select></label>
    <div class="form-actions">${slot?actionBtn('slot-remove','解除绑定',null,'btn danger-btn',`data-index="${index}"`):''}<button type="submit" class="btn primary">${icon('check')}保存绑定</button></div></form>`;
}
