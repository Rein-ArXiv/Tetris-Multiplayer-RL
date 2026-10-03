'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {JSDOM,VirtualConsole}=require('jsdom');
const root=path.resolve(__dirname,'../../docs/learn');
const dom=new JSDOM('<!doctype html><body></body>',{runScripts:'outside-only',virtualConsole:new VirtualConsole()});
const w=dom.window;w.structuredClone=structuredClone;
w.eval(fs.readFileSync(path.join(root,'vendor/mermaid.min.js'),'utf8'));
w.mermaid.initialize({startOnLoad:false,securityLevel:'strict',suppressErrorRendering:true});
w.eval(fs.readFileSync(path.join(root,'lessons.js'),'utf8'));
(async()=>{
 let count=0;const failures=[];
 for(const lesson of w.LEARNING_LESSONS.lessons)for(const section of lesson.sections){
  const container=w.document.createElement('div');container.innerHTML=section.html;
  for(const code of container.querySelectorAll('[data-diagram] code')){
   try{await w.mermaid.parse(code.textContent);++count;}
   catch(e){failures.push(lesson.id+' / '+section.title+' / '+String(e.message).slice(0,240));}
  }
 }
 assert.equal(failures.length,0,failures.join('\n'));
 console.log('Mermaid syntax verified',count,'lesson diagrams');dom.window.close();
})().catch(e=>{console.error(e);dom.window.close();process.exitCode=1;});
