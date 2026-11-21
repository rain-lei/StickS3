'use strict';

const watchEffects = ['准备','火焰命中','冰霜命中','格挡反击','未命中','展开护盾','恢复生命'];
function watchView() {
  return `${pageTitle('实机观战','用 USB 连接任意一台参战的 StickS3，即可看到双方战况。')}
    <div class="watch-status"><span class="dot offline"></span><strong id="watch-source">正在寻找设备</strong></div>
    <section class="panel watch-wait" id="watch-wait"><h2>等待魔杖</h2><p>在两台设备上创建并加入同一场决斗。</p></section>
    <section class="watch-arena" id="watch-arena" hidden>
      <div class="watch-fighters">${['房主','加入者'].map((name,i)=>`<article class="panel watch-fighter">
        <div class="watch-art">${art(i?'duel':'frostling','cover')}<div class="arena-shade"></div>
          <div class="watch-name"><h2>${name}</h2><strong data-shield></strong></div><span data-charge></span></div>
        <div class="watch-stats">${healthMeter('生命',100,100,i?'danger':'vital')}
        <div class="watch-spells">${skills.map(s=>`<div>${art(s.id)}<span>${s.name}</span><b></b></div>`).join('')}</div>
        <div class="watch-effect" data-effect></div></div>
      </article>`).join('')}</div><div class="panel watch-shot" id="watch-shot"></div></section>`;
}

function startWatch(main) {
  let stopped=false, timer, controller, lastRoom, lastEvents=[];
  const source=main.querySelector('#watch-source'), wait=main.querySelector('#watch-wait');
  const arena=main.querySelector('#watch-arena'), dot=source.previousElementSibling;
  const fighters=Array.from(main.querySelectorAll('.watch-fighter'));
  const status=document.querySelector('#board-status small');
  function waiting(title, detail) {
    wait.hidden=false; arena.hidden=true; lastRoom=null; lastEvents=[];
    wait.querySelector('h2').textContent=title;
    wait.querySelector('p').textContent=detail;
  }
  function update({port,state}) {
    dot.classList.toggle('offline',!state);
    source.textContent=state ? `USB · ${port}` : '等待 USB 数据';
    status.lastChild.textContent=state ? ` USB · ${port}` : '设备未连接';
    status.firstElementChild.classList.toggle('offline',!state);
    if (!state) return waiting('等待魔杖','请用数据线连接装有实时观战固件的 StickS3。');
    const [,scene,mode,room,me,host,guest,shot]=state;
    if (scene!==5 || mode!==1) {
      const names=['灵巢','训练场','单人试炼','联机房间','寻找房间','协作副本','现实机关'];
      return waiting(`设备当前在${names[scene] || '现实机关'}`,'在设备上进入双人决斗后，双方战况会自动显示。');
    }
    const id=(room & 65535).toString(16).toUpperCase().padStart(4,'0');
    source.textContent=`USB · ${port}  /  房间 #${id}  /  接入${me?'加入者':'房主'}`;
    arena.hidden=false; wait.hidden=true;
    if (lastRoom!==room) { lastRoom=room; lastEvents=[]; }
    [host,guest].forEach(([hp,layers,charge,effect,event,...cd],i)=>{
      const card=fighters[i], meter=card.querySelector('[role=meter]');
      meter.setAttribute('aria-valuenow',hp); meter.firstElementChild.style.width=hp+'%';
      card.querySelector('.health b').textContent=`${hp} / 100`;
      card.querySelector('[data-shield]').textContent=`盾 ${layers} / 3`;
      card.querySelector('[data-charge]').textContent=charge ? `蓄力 · ${skills[charge-1].name}` : '';
      card.classList.toggle('charging',Boolean(charge));
      card.querySelectorAll('.watch-spells b').forEach((label,k)=>{
        label.textContent=cd[k] ? (cd[k]/1000).toFixed(1)+' 秒' : '就绪';
        label.parentElement.classList.toggle('cooling',cd[k]>0);
      });
      card.querySelector('[data-effect]').textContent=event ? watchEffects[effect] : '';
      if (lastEvents[i]!==undefined && lastEvents[i]!==event && !matchMedia('(prefers-reduced-motion: reduce)').matches)
        card.querySelector('.watch-art').animate([{opacity:.55},{opacity:1}],{duration:360});
      lastEvents[i]=event;
    });
    main.querySelector('#watch-shot').textContent=!host[0] || !guest[0] ? `${host[0]?'房主':'加入者'}获胜` :
      shot[0] ? `${shot[1]?'加入者':'房主'}释放${skills[shot[2]].name} · ${shot[3]?'红外回探':'瞄准中'}` : '双方可自由施法';
  }
  async function poll() {
    controller=new AbortController();
    const timeout=setTimeout(()=>controller.abort(),1700);
    try {
      const response=await fetch('/api/live',{cache:'no-store',signal:controller.signal});
      if (!response.ok) throw Error();
      const data=await response.json();
      if (!stopped) update(data);
    } catch {
      if (!stopped) {
        update({port:null,state:null});
        source.textContent='本机观战服务未连接';
        waiting('启动本机观战','在工程目录运行 stick.cmd watch，然后打开终端显示的本机地址。');
      }
    } finally {
      clearTimeout(timeout);
      if (!stopped) timer=setTimeout(poll,150);
    }
  }
  poll();
  return () => {
    stopped=true; clearTimeout(timer); controller?.abort();
    status.lastChild.textContent='设备未连接'; status.firstElementChild.classList.add('offline');
  };
}
