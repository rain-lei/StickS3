(function riftStudio(){
  'use strict';
  const main = $('#content');
  const dialog = $('#dialog');
  const mobile = matchMedia('(max-width:760px)');
  const navItems = navGroups.flatMap(group => group.items);
  const availableRooms = ['duel','raid'];
  let route = { name:'home', detail:'' };
  let battle = null;
  let practiceMessage = '';
  let charge = null;
  let toastTimeout;
  let audio;
  let dialogTrigger = null;

  function renderIcons(element = document) {
    element.querySelectorAll('[data-icon]').forEach(node => { node.innerHTML = icon(node.dataset.icon); });
  }

  function getRoute() {
    const [name,detail] = location.hash.slice(1).replace(/^\//,'').split('/');
    if (name === 'arena' && Object.hasOwn(modes,detail)) return { name, detail };
    if (name === 'training') return { name, detail: skillById(detail).id };
    if (name === 'rooms') return { name, detail: availableRooms.includes(detail) ? detail : 'duel' };
    return { name: navItems.some(item => item[0] === name) ? name : 'home', detail: '' };
  }

  function navigation(active) {
    $('#navigation').innerHTML = navGroups.map(group => `<div class="nav-group"><p class="nav-caption">${group.name}</p><div class="nav-list">${group.items.map(([id,glyph,name])=>`<a class="nav-item ${active===id?'active':''}" href="${navUrl(id)}" ${active===id?'aria-current="page"':''}>${icon(glyph)}<span>${name}</span></a>`).join('')}</div></div>`).join('');
  }

  function updateSound() {
    const button = $('#sound');
    button.innerHTML = icon(progress.sound ? 'volume' : 'mute');
    button.setAttribute('aria-pressed', String(progress.sound));
    button.setAttribute('aria-label', '界面音效');
    button.title = progress.sound ? '关闭界面音效' : '开启界面音效';
  }

  function tone(frequency = 600) {
    if (!progress.sound) return;
    try {
      const Context = window.AudioContext || window.webkitAudioContext;
      if (!Context) return;
      audio ||= new Context();
      if (audio.state === 'suspended') audio.resume().catch(()=>{});
      const osc = audio.createOscillator();
      const gain = audio.createGain();
      const t = audio.currentTime;
      osc.type = 'sine';
      osc.frequency.setValueAtTime(frequency,t);
      osc.frequency.exponentialRampToValueAtTime(frequency*1.22,t+.12);
      gain.gain.setValueAtTime(.0001,t);
      gain.gain.exponentialRampToValueAtTime(.075,t+.015);
      gain.gain.exponentialRampToValueAtTime(.0001,t+.19);
      osc.connect(gain).connect(audio.destination);
      osc.start(t);
      osc.stop(t+.2);
      osc.onended = ()=>{ osc.disconnect(); gain.disconnect(); };
    } catch { /* Audio is optional when a browser or output device blocks it. */ }
  }

  function notify(message) {
    clearTimeout(toastTimeout);
    const toast = $('#toast');
    toast.textContent = message;
    toast.classList.add('visible');
    toastTimeout = setTimeout(()=>toast.classList.remove('visible'),2600);
  }

  function setMenu(open, restoreFocus = false) {
    document.body.classList.toggle('menu-open',open);
    $('#scrim').hidden = !open;
    $('.menu-btn').setAttribute('aria-expanded',String(open));
    $('.workspace-main').inert = open;
    if (open) $('#sidebar .active')?.focus();
    else if (restoreFocus) $('.menu-btn').focus();
  }

  function openDialog(html) {
    setMenu(false);
    dialogTrigger = document.activeElement;
    $('#dialog-body').innerHTML = html;
    dialog.showModal();
    const input = dialog.querySelector('input');
    if (input) input.focus();
  }

  function refresh(scroll = false) {
    resetCharge();
    const active = route.name === 'arena' ? (route.detail === 'solo' ? 'trials' : 'rooms') : route.name;
    const title = route.name === 'arena' ? modes[route.detail].title : navItems.find(item=>item[0]===active)[2];
    document.title = `${title} · 裂隙 RIFT`;
    $('#crumb').textContent = title;
    navigation(active);
    switch (route.name) {
      case 'training': main.innerHTML = trainingView(skillById(route.detail), practiceMessage); break;
      case 'trials': main.innerHTML = trialsView(); break;
      case 'rooms': main.innerHTML = roomsView(route.detail); break;
      case 'runes': main.innerHTML = runesView(); break;
      case 'arena':
        if (!battle || battle.mode !== route.detail) battle = newEncounter(route.detail);
        main.innerHTML = arenaView(battle);
        break;
      default: main.innerHTML = homeView();
    }
    if (scroll) {
      window.scrollTo(0,0);
      main.focus({ preventScroll:true });
    }
  }

  function routeChanged() {
    if (dialog.open) dialog.close();
    setMenu(false);
    practiceMessage = '';
    route = getRoute();
    refresh(true);
  }

  function setEnergy(value) {
    const bar = $('#energy-fill');
    if (!bar) return;
    bar.style.width = `${value}%`;
    bar.parentElement.setAttribute('aria-valuenow',String(value));
    $('#energy-number').textContent = `${value}%`;
    $('.spell-orbit').style.setProperty('--charge',value);
  }

  function resetCharge() {
    if (!charge) return;
    cancelAnimationFrame(charge.frame);
    charge = null;
    $('#cast-button')?.classList.remove('held');
    const label = $('#cast-button span');
    if (label) label.textContent = '按住蓄力，松开释放';
    setEnergy(0);
  }

  const chargeDuration = 1200;
  const energyAt = start => cap(Math.floor((performance.now()-start)*100/chargeDuration),0,100);

  function startCharge(source) {
    if (charge || route.name !== 'training') return;
    charge = { start: performance.now(), source, frame: 0 };
    $('#cast-button').classList.add('held');
    $('#cast-button span').textContent = '正在蓄力…';
    $('#practice-feedback').textContent = '';
    function update() {
      if (!charge) return;
      const energy = energyAt(charge.start);
      setEnergy(energy);
      if (energy >= 50) $('#cast-button span').textContent = '能量就绪，松开释放';
      if (energy < 100) charge.frame = requestAnimationFrame(update);
    }
    update();
  }

  function releaseCharge(source) {
    if (!charge || charge.source !== source) return;
    const energy = energyAt(charge.start);
    const skill = skillById(route.detail);
    resetCharge();
    if (energy < 50) {
      $('#practice-feedback').textContent = `能量 ${energy}%，蓄到 50% 后再松开。`;
      return;
    }
    finishPractice(skill);
    tone([480,700,525,600][skills.indexOf(skill)]);
    const mastery = progress.mastery[skills.indexOf(skill)];
    practiceMessage = `${skill.name}释放成功！+2 XP · ${rank(mastery)} ${mastery}/3`;
    refresh();
    $('.spell-orbit').classList.add('released');
    if (source === 'keyboard' || source === 'assistive') $('#cast-button').focus({preventScroll:true});
  }

  document.addEventListener('pointerdown',event=>{
    const button = event.target.closest('#cast-button');
    if (!button || event.button !== 0 || !event.isPrimary) return;
    button.setPointerCapture(event.pointerId);
    startCharge(event.pointerId);
  });
  document.addEventListener('pointerup',event=>releaseCharge(event.pointerId));
  document.addEventListener('pointercancel',event=>{ if (charge?.source === event.pointerId) resetCharge(); });
  document.addEventListener('lostpointercapture',event=>{ if (charge?.source === event.pointerId) resetCharge(); });
  window.addEventListener('blur',resetCharge);
  document.addEventListener('visibilitychange',()=>{ if (document.hidden) resetCharge(); });

  document.addEventListener('keydown',event=>{
    if (event.key === 'Escape' && document.body.classList.contains('menu-open')) {
      setMenu(false,true);
      return;
    }
    if (document.body.classList.contains('menu-open') && event.key === 'Tab') {
      const links = Array.from($('#sidebar').querySelectorAll('a,button')).filter(node => node.getClientRects().length);
      if (event.shiftKey && document.activeElement === links[0]) { event.preventDefault(); links.at(-1).focus(); }
      if (!event.shiftKey && document.activeElement === links.at(-1)) { event.preventDefault(); links[0].focus(); }
    }
    if (event.target.id === 'cast-button' && [' ','Enter'].includes(event.key)) {
      event.preventDefault();
      if (!event.repeat) startCharge('keyboard');
    }
  });
  document.addEventListener('keyup',event=>{
    if ([' ','Enter'].includes(event.key) && charge?.source === 'keyboard') {
      event.preventDefault();
      releaseCharge('keyboard');
    }
  });
  document.addEventListener('focusout',event=>{ if (event.target.id === 'cast-button' && charge?.source === 'keyboard') resetCharge(); });

  document.addEventListener('click',event=>{
    if (event.target.closest('.skip')) { event.preventDefault(); main.focus(); return; }
    if (event.target.closest('#cast-button') && event.detail === 0) {
      if (charge?.source === 'assistive') releaseCharge('assistive');
      else startCharge('assistive');
      return;
    }
    const button = event.target.closest('[data-act]');
    if (!button) {
      if (event.target.closest('a.nav-item,a.brand') && document.body.classList.contains('menu-open')) setMenu(false,true);
      return;
    }
    const { act, index, mode, skill } = button.dataset;
    const slotIndex = Number(index);
    switch (act) {
      case 'menu': setMenu(true); break;
      case 'menu-close': setMenu(false,true); break;
      case 'sound':
        progress.sound = !progress.sound;
        persist(); updateSound(); tone(640);
        notify(progress.sound?'界面音效已开启':'界面音效已关闭');
        break;
      case 'device': openDialog(deviceDialog()); break;
      case 'pet': openDialog(petDialog()); break;
      case 'help': openDialog(helpDialog()); break;
      case 'close': dialog.close(); break;
      case 'battle-start':
        if (!Object.hasOwn(modes, mode)) break;
        battle = newEncounter(mode);
        if (location.hash === navUrl('arena',mode)) refresh(true);
        else location.hash = navUrl('arena',mode);
        break;
      case 'battle-cast':
        if (!battle || battle.result) break;
        castInEncounter(battle, skill);
        tone(battle.result === 'win' ? 825 : 480);
        refresh();
        (battle.result ? main.querySelector('.settlement button') : main.querySelector(`[data-skill="${skill}"]`))?.focus({preventScroll:true});
        notify(battle.result ? (battle.result === 'win'?'挑战胜利，奖励已记录':'本场挑战结束') : battle.lines[0]);
        break;
      case 'slot-edit':
        if (Number.isInteger(slotIndex) && slotIndex >= 0 && slotIndex < 3) openDialog(slotDialog(slotIndex));
        break;
      case 'slot-remove':
        $('#dialog-body').innerHTML = `<div class="modal-pad">${dialogHeading('link','解除机关绑定',`将「${esc(progress.slots[slotIndex].name)}」从槽位 0${slotIndex+1} 移除。`)}<div class="form-actions">${actionBtn('slot-keep','保留绑定',null,'btn secondary',`data-index="${slotIndex}"`)}${actionBtn('slot-clear','确认解除',null,'btn danger-btn',`data-index="${slotIndex}"`)}</div></div>`;
        dialog.querySelector('[data-act=slot-keep]').focus();
        break;
      case 'slot-keep':
        $('#dialog-body').innerHTML = slotDialog(slotIndex);
        dialog.querySelector('input').focus();
        break;
      case 'slot-clear':
        progress.slots[slotIndex] = null;
        persist();
        dialog.close(); refresh(); focusSlot(slotIndex); notify('机关绑定已解除');
        break;
      case 'slot-trigger': {
        const slot = progress.slots[slotIndex];
        if (!slot) break;
        slot.used += 1;
        record('rune',`预演触发「${slot.name}」`);
        tone(755); refresh(); focusSlot(slotIndex,'slot-trigger');
        notify(`已模拟触发「${slot.name}」 · 第 ${slot.used} 次`);
        break;
      }
    }
  });

  function focusSlot(index, action = 'slot-edit') {
    main.querySelector(`[data-act="${action}"][data-index="${index}"]`)?.focus({preventScroll:true});
  }

  document.addEventListener('submit',event=>{
    if (event.target.id !== 'slot-form') return;
    event.preventDefault();
    const form = event.target;
    const nameInput = form.elements.namedItem('name');
    const name = nameInput.value.trim();
    if (!name) {
      nameInput.setCustomValidity('请输入一个动作名称');
      nameInput.reportValidity();
      return;
    }
    const index = Number(form.dataset.index);
    const type = form.elements.namedItem('type').value;
    const previous = progress.slots[index];
    progress.slots[index] = { name, type, used: previous?.name === name && previous.type === type ? previous.used : 0 };
    persist();
    dialog.close(); refresh(); focusSlot(index);
    notify(`已绑定「${name}」`);
  });
  document.addEventListener('input',event=>{
    if (event.target.matches('#slot-form input')) event.target.setCustomValidity('');
  });
  dialog.addEventListener('click',event=>{
    if (event.target === dialog) {
      const rect = dialog.getBoundingClientRect();
      if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) dialog.close();
    }
  });
  dialog.addEventListener('close',()=>{
    if (!dialogTrigger?.isConnected) return;
    if (mobile.matches && $('#sidebar').contains(dialogTrigger)) $('.menu-btn').focus({preventScroll:true});
    else dialogTrigger.focus({preventScroll:true});
    dialogTrigger = null;
  });
  mobile.addEventListener('change',()=>{ if (!mobile.matches) setMenu(false); });
  window.addEventListener('hashchange',routeChanged);
  renderIcons();
  updateSound();
  route = getRoute();
  refresh();
})();
