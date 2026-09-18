// Executes the actual rendered setup-page script; no hand-copied clock helper.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const source=fs.readFileSync(process.argv[2],'utf8');
const elements=new Map();
function element(id){
  if(!elements.has(id))elements.set(id,{disabled:false,hidden:false,textContent:'',files:[],value:'',checked:false,listeners:{},addEventListener(name,callback){this.listeners[name]=callback;}});
  return elements.get(id);
}
const requests=[];
let offsetWest=420,zone='America/Los_Angeles',replyOk=true,replyValid=true;
let epochMillis=1789651234567;
class BrowserDate extends Date{constructor(){super(epochMillis);}getTimezoneOffset(){return offsetWest;}}
const environment={Date:BrowserDate,Math,Error,console,setTimeout,clearTimeout,AbortController,
  Intl:{DateTimeFormat:()=>({resolvedOptions:()=>({timeZone:zone})})},
  document:{getElementById:element},
  fetch:async(path,options)=>{requests.push({path,...options,parsed:JSON.parse(options.body)});return {ok:replyOk,json:async()=>({ok:replyValid,valid:replyValid,message:replyOk?'Clock synchronized.':'Clock hardware unavailable.'})};}
};
const context=vm.createContext({...environment});
const settle=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  vm.runInContext(source,context);
  await settle();
  assert.equal(requests.length,1,'Page load syncs once without requiring profile Save');
  assert.equal(requests[0].path,'/clock');assert.equal(requests[0].method,'POST');
  assert.equal(requests[0].headers['Content-Type'],'application/json');
  assert.match(requests[0].headers['X-Conference-Nonce'],/^[a-f0-9]{32}$/);
  assert.deepEqual(requests[0].parsed,{epoch:1789651234,offset_minutes:-420,timezone:'America/Los_Angeles'});
  assert.match(element('clockStatus').textContent,/synchronized/);assert.equal(element('form').hidden,false);assert.equal(element('clockRetry').disabled,false);
  for(const [west,east] of [[420,-420],[-330,330],[-345,345],[0,0],[-840,840]]){
    offsetWest=west;const actual=JSON.parse(vm.runInContext('JSON.stringify(browserClockPayload())',context));
    assert.equal(actual.epoch,1789651234);assert.equal(actual.offset_minutes,east);
  }
  zone='Invalid zone';assert.equal(vm.runInContext('browserClockPayload().timezone',context),undefined);
  zone='Asia/Kathmandu';offsetWest=-345;
  replyOk=false;await element('clockRetry').listeners.click();await settle();
  assert.match(element('clockStatus').textContent,/could not be confirmed/);assert.match(element('clockStatus').textContent,/hardware unavailable/);
  assert.equal(element('save').disabled,false);assert.equal(element('cancel').disabled,false);
  replyOk=true;replyValid=false;await element('clockRetry').listeners.click();await settle();assert.match(element('clockStatus').textContent,/could not be confirmed/);
  epochMillis+=65000;
  replyValid=true;await element('clockRetry').listeners.click();await settle();
  assert.match(element('clockStatus').textContent,/synchronized/);assert.equal(requests.at(-1).parsed.offset_minutes,345);
  assert.equal(requests.at(-1).parsed.epoch,Math.floor(epochMillis/1000),'Retry samples fresh current time');
  assert.equal(requests.some(request=>request.path==='/save'),false,'Clock action never saves profile');
  const requestCount=requests.length;elements.clear();epochMillis+=3600000;
  vm.runInContext(source,vm.createContext({...environment}));await settle();
  assert.equal(requests.length,requestCount+1,'Reopening setup starts a new automatic clock request');
  assert.equal(requests.at(-1).parsed.epoch,Math.floor(epochMillis/1000),'Reopening samples fresh current time');
  assert.equal(element('form').hidden,false);
  console.log('Rendered portal clock script: automatic request, fresh retry/reopen time, nonce, epoch truncation, UTC offset sign, timezone validation and failure status passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
