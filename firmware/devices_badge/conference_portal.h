#pragma once
#include "conference_profile_store.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include "conference_transport.h"

enum class ConferencePortalOutcome : uint8_t {None,Saved,Cancelled,TimedOut,Error};

class ConferencePortal {
  static constexpr size_t IMAGE_LIMIT=128*1024,FORM_LIMIT=2048;
  ConferenceProfileStore &store_;ConferenceTransport transport_;DNSServer dns_;
  String ssid_,password_,nonce_,imageToken_,error_;
  bool active_=false,closeScheduled_=false;uint32_t deadline_=0,closeAt_=0;
  ConferencePortalOutcome outcome_=ConferencePortalOutcome::None;
  uint8_t *stagedImage_=nullptr;size_t stagedSize_=0;

  static String randomHex(unsigned bytes) {
    uint8_t random[16];esp_fill_random(random,sizeof(random));char encoded[33]={};
    if(bytes>16)bytes=16;
    for(unsigned i=0;i<bytes;i++)snprintf(encoded+i*2,3,"%02x",random[i]);return String(encoded);
  }
  static String escape(const String &input) {
    String s=input;s.replace("&","&amp;");s.replace("<","&lt;");s.replace(">","&gt;");s.replace("\"","&quot;");s.replace("'","&#39;");return s;
  }
  bool authorized(){return active_&&outcome_==ConferencePortalOutcome::None&&nonce_==transport_.request().nonce;}
  void respond(int code,const String &message){JsonDocument json;json["message"]=message;String body;serializeJson(json,body);transport_.reply(code,"application/json",body);}
  void freeStaged(){if(stagedImage_)free(stagedImage_);stagedImage_=nullptr;stagedSize_=0;imageToken_="";}
  void receiveHeaders(){
    const auto &request=transport_.request();
    if(!request.post){
      if(!strcmp(request.path,"/"))transport_.reply(200,"text/html; charset=utf-8",page());
      else transport_.reply(302,"text/plain","",true);
      return;
    }
    if(!authorized()){respond(403,"Open the current badge setup page.");return;}
    if(!strcmp(request.path,"/image"))freeStaged();
    if(!transport_.readBody())respond(503,"Not enough memory. Try a smaller image.");
  }
  void submit() {
    const String uri=transport_.request().path;
    if(!authorized()){respond(403,"Open the current badge setup page.");return;}
    if(uri=="/image"){
      int width=0,height=0;uint8_t *rgb=badgeDecodeAvatarJpeg(transport_.body(),transport_.bodyLength(),width,height);
      if(!rgb){respond(400,"Use a JPEG up to 512 by 512 and 128 KiB.");return;}
      badgeFreeAvatarPixels(rgb);stagedSize_=transport_.bodyLength();stagedImage_=transport_.takeBody();imageToken_=randomHex(8);
      JsonDocument result;result["message"]="Photo ready. Save badge to keep it.";result["imageToken"]=imageToken_;String body;serializeJson(result,body);transport_.reply(200,"application/json",body);return;
    }
    if(uri=="/cancel"){
      respond(200,"Cancelled. Your previous badge is unchanged. You may close this page.");outcome_=ConferencePortalOutcome::Cancelled;closeScheduled_=false;return;
    }
    JsonDocument json;DeserializationError parsed=deserializeJson(json,transport_.body(),transport_.bodyLength());
    if(parsed||!json.is<JsonObject>()){respond(400,"Invalid form. Reload and try again.");return;}
    const char *fields[]={"name","github","x","linkedin","image","imageToken"};
    for(const char *field:fields){
      if(!json[field].is<const char*>()){respond(400,"Incomplete form. Reload and try again.");return;}
      JsonString value=json[field].as<JsonString>();
      if(value.size()!=strlen(value.c_str())){respond(400,"Form fields cannot contain null characters.");return;}
    }
    for(JsonPair pair:json.as<JsonObject>()){
      bool known=false;for(const char *field:fields)if(strcmp(pair.key().c_str(),field)==0)known=true;
      if(!known){respond(400,"Unexpected form field.");return;}
    }
    ConferenceProfile candidate;
    if(!conferenceName(json["name"].as<String>(),candidate.name)){respond(400,"Use a name of at most 60 characters / 120 UTF-8 bytes, without control characters.");return;}
    for(unsigned i=0;i<3;i++)if(!conferenceSocialUrl(ConferenceNetwork(i),json[fields[i+1]].as<String>(),candidate.urls[i])){respond(400,"Check the social handles. Use a profile handle or its HTTPS profile URL.");return;}
    String imageAction=json["image"].as<String>();bool replace=false;const uint8_t *image=nullptr;size_t imageSize=0;
    if(imageAction=="remove")replace=true;
    else if(imageAction=="staged"){
      if(!stagedImage_||json["imageToken"].as<String>()!=imageToken_){respond(400,"Upload your photo again before saving.");return;}
      replace=true;image=stagedImage_;imageSize=stagedSize_;
    }else if(imageAction!="keep"){respond(400,"Invalid photo choice.");return;}
    if(!store_.save(candidate,image,imageSize,replace)){respond(500,store_.error());return;}
    // Queue the acknowledgment. The shutdown grace begins only after the
    // bounded response transport has finished or detected a failed connection.
    respond(200,"Saved on your badge. You may close this page; setup Wi-Fi will turn off.");outcome_=ConferencePortalOutcome::Saved;closeScheduled_=false;freeStaged();
  }
  String page() {
    const ConferenceProfile &profile=store_.profile();
    String html=R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>init() badge setup</title><style>body{font:17px system-ui;background:#101114;color:#fff;max-width:460px;margin:25px auto;padding:24px}h1{font-size:32px}label{display:block;margin:22px 0 7px}input,button{box-sizing:border-box;width:100%;padding:14px;border-radius:10px;border:1px solid #666;font:inherit}input{background:#24262b;color:#fff}button{margin-top:20px;background:#fff;color:#111;font-weight:650}button.secondary{background:#24262b;color:#fff}p{color:#ccc;line-height:1.5}.row{display:flex;gap:10px;align-items:center}.row input{width:auto}#status{min-height:3em;white-space:pre-wrap}small{color:#bbb}button:disabled{opacity:.5}</style></head><body><p>init() / local badge setup</p><h1>Make it yours.</h1><p>Saved only on this badge. No internet, accounts, or sign-in needed. Blank accounts stay available as placeholders.</p><form id="form"><label for="name">Display name</label><input id="name" maxlength="120" autocomplete="off" value=")HTML";
    html+=escape(profile.name)+"\"><label for=\"github\">GitHub handle or profile URL</label><input id=\"github\" maxlength=\"180\" autocomplete=\"off\" value=\""+escape(profile.urls[0])+"\"><label for=\"x\">X / Twitter handle or profile URL</label><input id=\"x\" maxlength=\"180\" autocomplete=\"off\" value=\""+escape(profile.urls[1])+"\"><label for=\"linkedin\">LinkedIn handle or /in/ profile URL</label><input id=\"linkedin\" maxlength=\"180\" autocomplete=\"off\" value=\""+escape(profile.urls[2])+"\">";
    html+=R"HTML(<label for="photo">Your photo</label><input id="photo" type="file" accept="image/jpeg,image/png,image/webp"><p id="photoState">)HTML";
    html+=store_.avatarPixels()?"Your saved photo will be kept unless you replace or remove it.":"No saved photo. A placeholder will appear until you add one.";
    html+=R"HTML(</p><small>Photos are converted on your phone to a JPEG, at most 512 × 512. The badge stores a square crop.</small><label class="row"><input id="remove" type="checkbox">Remove saved photo</label><button id="save" type="submit">Save badge</button><button id="cancel" type="button" class="secondary">Cancel setup</button></form><p id="status" role="status" aria-live="polite"></p><script>'use strict';const nonce=')HTML";
    html+=nonce_;
    html+=R"HTML(';const $=id=>document.getElementById(id);let busy=false;function state(message){$('status').textContent=message}function lock(value){busy=value;$('save').disabled=value;$('cancel').disabled=value}async function request(path,body,type='application/json'){const r=await fetch(path,{method:'POST',headers:{'Content-Type':type,'X-Conference-Nonce':nonce},body:type==='application/json'?JSON.stringify(body):body});const result=await r.json();if(!r.ok)throw Error(result.message||'Please try again.');return result}function decodePhoto(file){return new Promise((resolve,reject)=>{const url=URL.createObjectURL(file),img=new Image();img.onload=()=>{URL.revokeObjectURL(url);resolve(img)};img.onerror=()=>{URL.revokeObjectURL(url);reject(Error('Use a JPEG, PNG, or WebP photo.'))};img.src=url})}function jpegBlob(canvas,quality){return new Promise(resolve=>canvas.toBlob(resolve,'image/jpeg',quality))}async function photo(file){if(file.size>10*1024*1024)throw Error('Choose a photo smaller than 10 MB.');const img=await decodePhoto(file);if(!img.width||!img.height)throw Error('This photo is empty.');const canvas=document.createElement('canvas');const scale=Math.min(1,512/Math.max(img.width,img.height));canvas.width=Math.max(1,Math.round(img.width*scale));canvas.height=Math.max(1,Math.round(img.height*scale));const ctx=canvas.getContext('2d');ctx.fillStyle='#000';ctx.fillRect(0,0,canvas.width,canvas.height);ctx.drawImage(img,0,0,canvas.width,canvas.height);let blob;for(const quality of [.88,.72,.55,.38]){blob=await jpegBlob(canvas,quality);if(blob&&blob.size<=128*1024)return blob}throw Error('This photo is too detailed. Try a smaller photo.')}function finish(message){state(message);$('form').hidden=true} $('photo').addEventListener('change',()=>{if($('photo').files.length){$('remove').checked=false;$('photoState').textContent='New photo selected. Save badge to keep it.'}});$('remove').addEventListener('change',()=>{if($('remove').checked)$('photo').value=''});$('form').addEventListener('submit',async event=>{event.preventDefault();if(busy)return;lock(true);state('Saving…');try{let image=$('remove').checked?'remove':'keep',imageToken='';const file=$('photo').files[0];if(file&&!$('remove').checked){state('Preparing photo on your phone…');const blob=await photo(file);state('Sending photo to your badge…');const result=await request('/image',blob,'image/jpeg');image='staged';imageToken=result.imageToken}const result=await request('/save',{name:$('name').value,github:$('github').value,x:$('x').value,linkedin:$('linkedin').value,image,imageToken});finish(result.message)}catch(error){state(error.message+' If the connection was lost, check your badge or reopen setup to confirm what was saved.');lock(false)}});$('cancel').addEventListener('click',async()=>{if(busy)return;lock(true);try{const result=await request('/cancel',{});finish(result.message)}catch(error){state(error.message+' You can also press either badge pusher to leave setup.');lock(false)}});</script></body></html>)HTML";
    return html;
  }
public:
  explicit ConferencePortal(ConferenceProfileStore &store):store_(store),transport_([this](){receiveHeaders();},[this](){submit();}){}
  ~ConferencePortal(){stop();}
  ConferencePortal(const ConferencePortal&)=delete;
  ConferencePortal&operator=(const ConferencePortal&)=delete;
  bool start(const char *testPassword=nullptr) {
    if(active_)return true;
    error_="";outcome_=ConferencePortalOutcome::None;closeAt_=0;closeScheduled_=false;
    if(!store_.ready()){error_=store_.error();outcome_=ConferencePortalOutcome::Error;return false;}
    password_=testPassword?String(testPassword):randomHex(6);
    if(password_.length()<12||password_.length()>32){password_="";error_="Invalid temporary setup password";outcome_=ConferencePortalOutcome::Error;return false;}
    for(size_t i=0;i<password_.length();i++)if(!conferenceAlnum(password_[i])){password_="";error_="Invalid temporary setup password";outcome_=ConferencePortalOutcome::Error;return false;}
    nonce_=randomHex(16);char suffix[13];uint64_t chip=ESP.getEfuseMac();snprintf(suffix,sizeof(suffix),"%012llx",static_cast<unsigned long long>(chip&0xffffffffffffULL));ssid_=String("init-badge-")+suffix;
    WiFi.persistent(false);WiFi.setAutoReconnect(false);WiFi.disconnect(false,false);WiFi.mode(WIFI_AP);
    bool configured=WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
    if(!configured||!WiFi.softAP(ssid_.c_str(),password_.c_str(),1,0,2)){
      WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);password_="";nonce_="";error_="Could not start setup Wi-Fi";outcome_=ConferencePortalOutcome::Error;return false;
    }
    if(!dns_.start(53,"*",WiFi.softAPIP())){WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);password_="";nonce_="";error_="Could not start setup portal";outcome_=ConferencePortalOutcome::Error;return false;}
    if(!transport_.begin()){dns_.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);password_="";nonce_="";error_="Could not start setup server";outcome_=ConferencePortalOutcome::Error;return false;}
    active_=true;deadline_=millis()+600000;return true;
  }
  void tick(){
    if(!active_)return;
    // Apply the session deadline before accepting another byte of an upload.
    if(outcome_==ConferencePortalOutcome::None&&int32_t(millis()-deadline_)>=0){outcome_=ConferencePortalOutcome::TimedOut;stop();return;}
    dns_.processNextRequest();
    if(!closeScheduled_)transport_.tick();
    if(outcome_!=ConferencePortalOutcome::None){
      if(!closeScheduled_&&transport_.idle()){closeAt_=millis()+3000;closeScheduled_=true;}
      if(closeScheduled_&&int32_t(millis()-closeAt_)>=0)stop();
    }
  }
  void stop(){
    transport_.stop();
    if(active_){if(outcome_==ConferencePortalOutcome::None)outcome_=ConferencePortalOutcome::Cancelled;dns_.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);active_=false;}
    freeStaged();password_="";nonce_="";closeAt_=0;closeScheduled_=false;
  }
  bool active()const{return active_;}
  const String &ssid()const{return ssid_;}
  const String &password()const{return password_;}
  const String &error()const{return error_;}
  ConferencePortalOutcome outcome()const{return outcome_;}
  const ConferenceTransport::Diagnostics &transportDiagnostics()const{return transport_.diagnostics();}
};
