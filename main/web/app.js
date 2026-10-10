const $=(id)=>document.getElementById(id);
let pick="", cap=7.5, otaErr=false;
document.querySelectorAll(".nav:not([disabled])").forEach(b=>b.onclick=()=>{
  document.querySelectorAll(".nav").forEach(x=>x.classList.remove("on"));
  b.classList.add("on");
  document.querySelectorAll("main section").forEach(s=>s.classList.add("hide"));
  $(b.dataset.tab).classList.remove("hide");
  if (b.dataset.tab==="logs") logsMeta();
});
async function tick(){
  try{
    const s=await (await fetch("/api/status?t="+Date.now())).json();
    const live=s.ccs_voltage_0p1v>0;
    $("v").textContent=live?(s.ccs_voltage_0p1v/10).toFixed(0)+" В":"--";
    $("a").textContent=live?(Math.abs(s.ccs_current_0p1a)/10).toFixed(1)+" А":"--";
    $("p").textContent=live?(s.power_w||0)+" Вт":"--";
    const kwh=Number(s.energy_kwh||0);
    $("kwh").textContent=kwh.toFixed?kwh.toFixed(3)+" кВт·ч":"--";
    $("kwh2").textContent=$("kwh").textContent;
    if (s.cap_kwh) cap=Number(s.cap_kwh);
    $("soc").textContent=(s.soc||0)+" %";
    $("soc2").textContent=(s.soc||0)+" %";
    $("st").textContent=s.main_state_name||"нет сессии";
    const tx = s.tx_enabled ? "передача вкл" : "передача выкл";
    $("xchg").textContent = tx+" · "+(s.charger_online?("обмен "+s.rx_count+"/"+s.tx_count):"тишина");
    $("proto").textContent=(s.protocol||"AUTO")+" · "+(s.stage||"");
    if (document.activeElement !== $("protosel")) {
      const map = {"AUTO":"0","GBT-2015/A":"1","GBT-2011":"2","GBT-2023-B":"3"};
      if (map[s.protocol] !== undefined) $("protosel").value = map[s.protocol];
    }
    $("stage").textContent=s.stage||"--";
    $("mins").textContent=(s.ccs_time_min||0)+" мин";
    $("lim").textContent=s.cml_max_voltage_0p1v?((s.cml_min_voltage_0p1v/10)+"-"+(s.cml_max_voltage_0p1v/10)+" В"):"ждём CML";
    $("ch").textContent=s.ap_channel||"--";
    $("sta").textContent=s.ap_clients||0;
    $("sip").textContent=s.sta_ip&&s.sta_ip!=="wait"?("http://"+s.sta_ip+"/"):"ещё нет";
    $("req").textContent=((s.bcl_voltage_0p1v||2400)/10)+" В / "+(Math.abs(s.bcl_current_0p1a||500)/10)+" А";
    $("rel").textContent=(s.relay_a?"замкн":"откр")+" / "+(s.relay_b?"замкн":"откр");
    $("vin").textContent=s.vin||"не задан";
    $("capv").textContent=cap+" кВт·ч";
    if ($("p2011")) {
      const on = (name, yes) => { $(name).textContent = yes ? "активен" : "не активен"; };
      on("p2011", s.protocol==="GBT-2011");
      on("p2015", s.protocol==="GBT-2015/A");
      on("p2023", s.protocol==="GBT-2023-B");
      $("plock").textContent = (s.proto_lock ? "зафиксирован вручную" : "авто") + " · " + (s.protocol||"AUTO");
      const seen = (ms) => ms && ms>0;
      $("crmv").textContent = !seen(s.last_crm_ms) ? "кадра не было" : (s.crm_status===0xAA ? "0xAA — узнала батарею" : "0x00 — ещё не узнала, шлём BRM");
      $("crov").textContent = !seen(s.last_cro_ms) ? "кадра не было" : (s.cro_status_raw===0xAA ? "0xAA — станция готова" : "0x00 — контакторы ещё не готовы");
      $("ccsv").textContent = !seen(s.last_ccs_ms) ? "кадра не было" : ((s.ccs_charge_permit===1 ? "можно ток" : "пауза, сессию не рвём")+" · "+(s.ccs_time_min||0)+" мин");
      if (!seen(s.last_cml_ms)) $("cmlv").textContent = "кадра не было";
      else {
        const amax = Math.abs(s.cml_max_current_0p1a||0)/10;
        const amin = Math.abs(s.cml_min_current_0p1a||0)/10;
        $("cmlv").textContent = ((s.cml_min_voltage_0p1v||0)/10)+"-"+((s.cml_max_voltage_0p1v||0)/10)+" В, ток "+amin.toFixed(0)+"-"+amax.toFixed(0)+" А";
      }
      $("cstv").textContent = !seen(s.last_cst_ms) ? "кадра не было" : ("причина 0x"+(s.cst_status_raw||0).toString(16));
      $("csdv").textContent = !seen(s.last_csd_ms) ? "кадра не было" : ((s.csd_time_min||0)+" мин, "+((s.csd_energy_0p1kwh||0)/10).toFixed(1)+" кВт·ч по станции");
      $("cemv").textContent = !seen(s.last_cem_ms) ? "кадра не было" : ("байты "+(s.cem||""));
      const mode = s.bcl_mode ? "постоянный ток" : "постоянное напряжение";
      $("bclv").textContent = ((s.bcl_voltage_0p1v||0)/10)+" В / "+(Math.abs(s.bcl_current_0p1a||0)/10)+" А · "+mode;
    }
    if (!otaErr && typeof s.ota_pct === "number" && s.ota_pct >= 0) animate(s.ota_pct, "куб "+s.ota_pct+"%");
  }catch(e){}
}
setInterval(tick,1000); tick();
$("set").onsubmit=async(e)=>{e.preventDefault(); cap=+$("cap").value||7.5;
  await fetch("/api/setpoints",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"v="+$("vv").value+"&i="+$("ii").value+"&soc="+$("ss").value+"&cap="+$("cap").value+"&proto="+$("protosel").value+"&vin="+encodeURIComponent($("vinin").value)});
};
$("scan").onclick=async()=>{
  const r=await (await fetch("/api/wifi/scan")).json();
  $("nets").innerHTML=""; $("sheet").classList.add("hide");
  (r.nets||[]).forEach(n=>{const li=document.createElement("li"); li.textContent=n; li.onclick=()=>{pick=n; $("chosen").textContent=n; $("sheet").classList.remove("hide"); $("pw").focus();}; $("nets").appendChild(li);});
};
$("cancel").onclick=()=>$("sheet").classList.add("hide");
$("join").onclick=async()=>{ $("sip").textContent="wait"; $("sheet").classList.add("hide"); await fetch("/api/wifi/join",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"ssid="+encodeURIComponent(pick)+"&pass="+$("pw").value}); };
$("relay").onclick=async()=>{await fetch("/api/settings",{method:"POST",headers:{"Content-Type":"application/json"},body:"{\"relay_test\":true}"});};
$("orig").onclick=async()=>{await fetch("/api/boot/original",{method:"POST"});};
async function logsMeta(){
  try{
    const r=await (await fetch("/api/logs?t="+Date.now())).json();
    const logs=r.logs||[];
    $("logn").textContent = logs.length ? ("файлов сессии: "+logs.length) : "файлов ещё нет: станция не начала обмен";
    $("allzip").classList.toggle("hide", logs.length<2);
    const ul=$("logfiles");
    ul.innerHTML="";
    logs.forEach((f)=>{
      const li=document.createElement("li");
      const a=document.createElement("a");
      a.href="/api/logs/file?n="+(f.n-1);
      a.textContent="Скачать этот файл";
      li.appendChild(document.createTextNode("№"+f.seq+" · "+f.protocol+" · "+f.bytes+" байт · "+f.reason+" "));
      li.appendChild(a);
      ul.appendChild(li);
    });
  }catch(e){}
}
$("getlog").onclick=async()=>{ $("esplog").textContent=await (await fetch("/api/log")).text(); $("canlog").textContent=await (await fetch("/api/can")).text(); logsMeta(); };
$("allzip").onclick=()=>{ location.href="/api/logs.zip"; };
let hold;
$("hold").addEventListener("pointerdown",(e)=>{e.preventDefault(); hold=setTimeout(async()=>{await fetch("/api/stop",{method:"POST"});},700);});
$("hold").addEventListener("pointerup",()=>clearTimeout(hold));
function animate(p,txt){ $("otamodal").classList.add("show"); $("bar").style.width=Math.max(0,Math.min(100,p))+"%"; $("otatxt").textContent=txt; }
$("ota").onclick=()=>{
  const f=$("bin").files[0];
  otaErr=false;
  if(!f){ otaErr=true; animate(0, "выберите файл .bin"); return; }
  const name=(f.name||"").toLowerCase();
  if(name.endsWith(".zip") || f.size>0x190000 || f.size<24){
    otaErr=true;
    animate(0, "нужен release\\FGMDCharge_FD_ROUND_V2.bin, не zip");
    return;
  }
  const xhr=new XMLHttpRequest(); xhr.timeout=0; xhr.open("POST","/api/ota");
  xhr.upload.onprogress=(e)=>{ if(e.lengthComputable) animate(e.loaded/e.total*100, "прошивка "+Math.round(e.loaded/e.total*100)+"%"); };
  xhr.onload=()=>{
    let t=xhr.responseText||"";
    if(xhr.status>=200 && xhr.status<300 && t.indexOf('"ok":true')>=0) animate(100, "перезагрузка, не снимайте питание");
    else { otaErr=true; animate(0, t||("ошибка "+xhr.status)); }
  };
  xhr.onerror=()=>{ otaErr=true; animate(0, "сеть оборвалась"); };
  xhr.ontimeout=()=>{ otaErr=true; animate(0, "таймаут"); };
  xhr.send(f);
};
$("netfw").onclick=async()=>{
  animate(8, "запрос файла, полоса на кубе");
  const body="url="+encodeURIComponent($("url").value)+"&user="+encodeURIComponent($("wu").value)+"&pass="+encodeURIComponent($("wp").value);
  const r=await fetch("/api/ota/url",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});
  animate(12, await r.text());
};
